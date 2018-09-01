#include <golos/plugins/private_message/private_message_plugin.hpp>
#include <golos/plugins/private_message/private_message_evaluators.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/private_message/private_message_exceptions.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <appbase/application.hpp>

#include <golos/chain/index.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>
#include <golos/chain/generic_custom_operation_interpreter.hpp>

#include <fc/smart_ref_impl.hpp>

#include <mutex>

//
template<typename T>
T dejsonify(const std::string &s) {
    return fc::json::from_string(s).as<T>();
}

#define LOAD_VALUE_SET(options, name, container, type) \
if( options.count(name) ) { \
    const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
    std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), &dejsonify<type>); \
}
//

namespace golos { namespace plugins { namespace private_message {

    struct callback_info final {
        callback_query query;
        std::shared_ptr<json_rpc::msg_pack> msg;

        callback_info() = default;
        callback_info(callback_query&& q, std::shared_ptr<json_rpc::msg_pack> m)
            : query(q),
              msg(m) {

        }
    };

    class private_message_plugin::private_message_plugin_impl final {
    public:
        private_message_plugin_impl(private_message_plugin& plugin)
            : db_(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {

            custom_operation_interpreter_ = std::make_shared
                    <generic_custom_operation_interpreter<private_message::private_message_plugin_operation>>(db_);

            auto coi = custom_operation_interpreter_.get();

            using impl = private_message_plugin_impl;

            coi->register_evaluator<private_message_evaluator<impl>>(this);
            coi->register_evaluator<private_delete_message_evaluator<impl>>(this);
            coi->register_evaluator<private_mark_message_evaluator<impl>>(this);
            coi->register_evaluator<private_settings_evaluator<impl>>(this);
            coi->register_evaluator<private_contact_evaluator<impl>>(this);

            db_.set_custom_operation_interpreter(plugin.name(), custom_operation_interpreter_);
        }

        template <typename Direction, typename Filter>
        std::vector<message_api_object> get_message_box(
            const std::string& account, const message_box_query&, Filter&&) const;

        std::vector<message_api_object> get_thread(
            const std::string& from, const std::string& to, const message_thread_query&) const;

        settings_api_object get_settings(const std::string& owner) const;

        contact_api_object get_contact_item(const contact_object& o) const;

        contact_api_object get_contact_info(const std::string& owner, const std::string& contact) const;

        contacts_size_api_object get_contacts_size(const std::string& owner) const;

        std::vector<contact_api_object> get_contacts(
            const std::string& owner, const private_contact_type, uint16_t limit, uint32_t offset) const;

        void call_callbacks(
            const callback_event_type, const account_name_type& from, const account_name_type& to, fc::variant);

        bool can_call_callbacks() const;

        ~private_message_plugin_impl() = default;

        bool is_tracked_account(account_name_type) const;

        std::shared_ptr<generic_custom_operation_interpreter<private_message_plugin_operation>> custom_operation_interpreter_;
        flat_map<std::string, std::string> tracked_account_ranges_;
        flat_set<std::string> tracked_account_list_;

        golos::chain::database& db_;

        std::mutex callbacks_mutex_;
        std::list<callback_info> callbacks_;
    };

    static inline time_point_sec min_create_date() {
        return time_point_sec(1);
    }

    template <typename Direction, typename GetAccount>
    std::vector<message_api_object> private_message_plugin::private_message_plugin_impl::get_message_box(
        const std::string& to, const message_box_query& query, GetAccount&& get_account
    ) const {
        std::vector<message_api_object> result;
        const auto& idx = db_.get_index<message_index>().indices().get<Direction>();
        auto newest_date = query.newest_date;

        if (newest_date == time_point_sec::min()) {
            newest_date = db_.head_block_time();
        }

        auto itr = idx.lower_bound(std::make_tuple(to, newest_date));
        auto etr = idx.upper_bound(std::make_tuple(to, min_create_date()));
        auto offset = query.offset;

        auto filter = [&](const message_object& o) -> bool {
            auto& account = get_account(o);
            return
                (query.select_accounts.empty() || query.select_accounts.count(account)) &&
                (query.filter_accounts.empty() || !query.filter_accounts.count(account)) &&
                (!query.unread_only || o.read_date == time_point_sec::min());
        };

        for (; itr != etr && offset; ++itr) {
            if (filter(*itr)){
                --offset;
            }
        }

        auto limit = query.limit;

        if (!limit) {
            limit = PRIVATE_DEFAULT_LIMIT;
        }

        result.reserve(limit);
        for (; itr != etr && result.size() < limit; ++itr) {
            if (filter(*itr)) {
                result.emplace_back(*itr);
            }
        }

        return result;
    }

     std::vector<message_api_object> private_message_plugin::private_message_plugin_impl::get_thread(
        const std::string& from, const std::string& to, const message_thread_query& query
    ) const {

        std::vector<message_api_object> result;
        const auto& outbox_idx = db_.get_index<message_index>().indices().get<by_outbox_account>();
        const auto& inbox_idx = db_.get_index<message_index>().indices().get<by_inbox_account>();

        auto outbox_itr = outbox_idx.lower_bound(std::make_tuple(from, to, query.newest_date));
        auto outbox_etr = outbox_idx.upper_bound(std::make_tuple(from, to, min_create_date()));
        auto inbox_itr = inbox_idx.lower_bound(std::make_tuple(from, to, query.newest_date));
        auto inbox_etr = inbox_idx.upper_bound(std::make_tuple(from, to, min_create_date()));
        auto offset = query.offset;

        auto filter = [&](const message_object& o) {
            return (!query.unread_only || o.read_date == time_point_sec::min());
        };

        auto itr_to_message = [&](auto& itr) -> const message_object& {
            const message_object& result = *itr;
            ++itr;
            return result;
        };

        auto select_message = [&]() -> const message_object& {
            if (outbox_itr != outbox_etr) {
                if (inbox_itr == inbox_etr || outbox_itr->id > inbox_itr->id) {
                    return itr_to_message(outbox_itr);
                }
            }
            return itr_to_message(inbox_itr);
        };

        auto is_not_done = [&]() -> bool {
            return outbox_itr != outbox_etr || inbox_itr != inbox_etr;
        };

        while (is_not_done() && offset) {
            auto& message = select_message();
            if (filter(message)){
                --offset;
            }
        }

        result.reserve(query.limit);
        while (is_not_done() && result.size() < query.limit) {
            auto& message = select_message();
            if (filter(message)) {
                result.emplace_back(message);
            }
        }

        return result;
    }

    settings_api_object private_message_plugin::private_message_plugin_impl::get_settings(
        const std::string& owner
    ) const {
        const auto& idx = db_.get_index<settings_index>().indices().get<by_owner>();
        auto itr = idx.find(owner);
        if (itr != idx.end()) {
            return settings_api_object(*itr);
        }

        return settings_api_object();
    }

    contact_api_object private_message_plugin::private_message_plugin_impl::get_contact_item(
        const contact_object& o
    ) const {
        contact_api_object result(o);

        const auto& idx = db_.get_index<contact_index>().indices().get<by_contact>();
        auto itr = idx.find(std::make_tuple(o.contact, o.owner));

        if (idx.end() != itr) {
            result.remote_type = itr->type;
        }

        return result;
    }

    contact_api_object private_message_plugin::private_message_plugin_impl::get_contact_info(
        const std::string& owner, const std::string& contact
    ) const {
        const auto& idx = db_.get_index<contact_index>().indices().get<by_contact>();
        auto itr = idx.find(std::make_tuple(owner, contact));

        if (itr != idx.end()) {
            return get_contact_item(*itr);
        }
        return contact_api_object();
    }

    contacts_size_api_object private_message_plugin::private_message_plugin_impl::get_contacts_size(
        const std::string& owner
    ) const {
        contacts_size_api_object result;

        const auto& idx = db_.get_index<contact_size_index>().indices().get<by_owner>();
        auto itr = idx.lower_bound(std::make_tuple(owner, unknown));
        auto etr = idx.upper_bound(std::make_tuple(owner, private_contact_type_size));

        for (; etr != itr; ++itr) {
            result.size[itr->type] = itr->size;
        }

        for (uint8_t i = unknown; i < private_contact_type_size; ++i) {
            auto t = static_cast<private_contact_type>(i);
            if (!result.size.count(t)) {
                result.size[t] = contacts_size_info();
            }
        }

        return result;
    }

    std::vector<contact_api_object> private_message_plugin::private_message_plugin_impl::get_contacts(
        const std::string& owner, const private_contact_type type, uint16_t limit, uint32_t offset
    ) const {
        std::vector<contact_api_object> result;

        result.reserve(limit);

        const auto& idx = db_.get_index<contact_index>().indices().get<by_owner>();
        auto itr = idx.lower_bound(std::make_tuple(owner, type));
        auto etr = idx.upper_bound(std::make_tuple(owner, type));

        for (; itr != etr && offset; ++itr, --offset);

        for (; itr != etr; ++itr) {
            result.push_back(get_contact_item(*itr));
        }
        return result;
    }

    bool private_message_plugin::private_message_plugin_impl::can_call_callbacks() const {
        return !db_.is_producing() && !db_.is_generating() && !callbacks_.empty();
    }

    void private_message_plugin::private_message_plugin_impl::call_callbacks(
        const callback_event_type event, const account_name_type& from, const account_name_type& to, fc::variant r
    ) {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        for (auto itr = callbacks_.begin(); callbacks_.end() != itr; ) {
            auto& info = *itr;

            if (info.query.filter_events.count(event) ||
                (!info.query.select_events.empty() && !info.query.select_events.count(event)) ||
                info.query.filter_accounts.count(from) ||
                info.query.filter_accounts.count(to) ||
                (!info.query.select_accounts.empty() &&
                 !info.query.select_accounts.count(to) &&
                 !info.query.select_accounts.count(from))
            ) {
                ++itr;
                continue;
            }

            try {
                info.msg->unsafe_result(r);
                ++itr;
            } catch (...) {
                callbacks_.erase(itr++);
            }
        }
    }

    template <typename Impl>
    void private_message_evaluator<Impl>::do_apply(const private_message_operation& pm) {
        if (!impl_->is_tracked_account(pm.from) && !impl_->is_tracked_account(pm.to)) {
            return;
        }

        database& d = impl_->db_;
        auto& contact_idx = d.get_index<contact_index>().indices().get<by_contact>();
        auto contact_itr = contact_idx.find(std::make_tuple(pm.to, pm.from));

        auto& cfg_idx = d.get_index<settings_index>().indices().get<by_owner>();
        auto cfg_itr = cfg_idx.find(pm.to);

        d.get_account(pm.to);

        GOLOS_CHECK_LOGIC(contact_itr == contact_idx.end() || contact_itr->type != ignored,
            logic_errors::sender_in_ignore_list,
            "Sender is in the ignore list of recipient");

        GOLOS_CHECK_LOGIC(
            (cfg_itr == cfg_idx.end() || !cfg_itr->ignore_messages_from_unknown_contact) ||
            (contact_itr != contact_idx.end() && contact_itr->type == pinned),
            logic_errors::recepient_ignores_messages_from_unknown_contact,
            "Recipient accepts messages only from his contact list");

        auto& id_idx = d.get_index<message_index>().indices().get<by_nonce>();
        auto id_itr = id_idx.find(std::make_tuple(pm.from, pm.to, pm.nonce));

        if (pm.update && id_itr == id_idx.end()) {
            GOLOS_THROW_MISSING_OBJECT("private_message",
                fc::mutable_variant_object()("from", pm.from)("to", pm.to)("nonce", pm.nonce));
        } else if (!pm.update && id_itr != id_idx.end()){
            GOLOS_THROW_OBJECT_ALREADY_EXIST("private_message",
                fc::mutable_variant_object()("from", pm.from)("to", pm.to)("nonce", pm.nonce));
        }

        auto now = d.head_block_time();

        auto set_message = [&](message_object& pmo) {
            pmo.from_memo_key = pm.from_memo_key;
            pmo.to_memo_key = pm.to_memo_key;
            pmo.checksum = pm.checksum;
            pmo.read_date = time_point_sec::min();
            pmo.receive_date = now;
            pmo.encrypted_message.resize(pm.encrypted_message.size());
            std::copy(
                pm.encrypted_message.begin(), pm.encrypted_message.end(),
                pmo.encrypted_message.begin());
        };

        if (id_itr == id_idx.end()) {
            d.create<message_object>([&](message_object& pmo) {
                pmo.from = pm.from;
                pmo.to = pm.to;
                pmo.nonce = pm.nonce;
                pmo.inbox_create_date = now;
                pmo.outbox_create_date = now;
                pmo.remove_date = time_point_sec::min();
                set_message(pmo);
            });
            id_itr = id_idx.find(std::make_tuple(pm.from, pm.to, pm.nonce));
        } else {
            d.modify(*id_itr, set_message);
        }

        if (this->impl_->can_call_callbacks()) {
            this->impl_->call_callbacks(
                callback_event_type::message, pm.from, pm.to,
                fc::variant(callback_message_event({callback_event_type::message, message_api_object(*id_itr)})));
        }

        // Ok, now update contact lists and counters in them
        auto& size_idx = d.get_index<contact_size_index>().indices().get<by_owner>();

        // Increment counters depends on side of communication
        auto inc_counters = [&](auto& o, const bool is_send) {
            if (is_send) {
                o.total_outbox_messages++;
                o.unread_outbox_messages++;
            } else {
                o.total_inbox_messages++;
                o.unread_inbox_messages++;
            }
        };

        // Update global counters by type of contact
        auto modify_size = [&](auto& owner, auto type, const bool is_new_contact, const bool is_send) {
            auto modify_counters = [&](auto& pcso) {
                inc_counters(pcso.size, is_send);
                if (is_new_contact) {
                    pcso.size.total_contacts++;
                }
            };

            auto size_itr = size_idx.find(std::make_tuple(owner, type));
            if (size_idx.end() == size_itr) {
                d.create<contact_size_object>([&](auto& pcso){
                    pcso.owner = owner;
                    pcso.type = type;
                    modify_counters(pcso);
                });
            } else {
                d.modify(*size_itr, modify_counters);
            }
        };

        // Add contact list if it doesn't exist or update it if it exits
        auto modify_contact = [&](auto& owner, auto& contact, auto type, const bool is_send) {
            bool is_new_contact;
            auto contact_itr = contact_idx.find(std::make_tuple(owner, contact));
            if (contact_idx.end() != contact_itr) {
                d.modify(*contact_itr, [&](auto& pco) {
                    inc_counters(pco.size, is_send);
                });
                is_new_contact = false;
                type = contact_itr->type;
            } else {
                d.create<contact_object>([&](auto& pco) {
                    pco.owner = owner;
                    pco.contact = contact;
                    pco.type = type;
                    inc_counters(pco.size, is_send);
                });
                is_new_contact = true;

                if (this->impl_->can_call_callbacks()) {
                    contact_itr = contact_idx.find(std::make_tuple(owner, contact));
                    this->impl_->call_callbacks(
                        callback_event_type::contact, owner, contact,
                        fc::variant(callback_contact_event(
                            {callback_event_type::contact, contact_api_object(*contact_itr)})));
                }
            }
            modify_size(owner, type, is_new_contact, is_send);
        };

        modify_contact(pm.from, pm.to, pinned, true);
        modify_contact(pm.to, pm.from, unknown, false);
    }

    template <typename Direction, typename Operation, typename Action, typename... Args>
    bool process_private_messages(database& db, const Operation& po, Action&& action, Args&&... args) {
        auto start_date = std::max(po.start_date, min_create_date());
        auto stop_date = std::max(po.stop_date, min_create_date());

        auto& idx = db.get_index<message_index>().indices().get<Direction>();
        auto itr = idx.lower_bound(std::make_tuple(std::forward<Args>(args)..., stop_date));
        auto etr = idx.lower_bound(std::make_tuple(std::forward<Args>(args)..., start_date));

        if (itr == etr) {
            return false;
        }

        while (itr != etr) {
            auto& message = (*itr);
            ++itr;
            if (!action(message)) {
                break;
            }
        }
        return true;
    }

    template <typename Operation, typename Map, typename ProcessAction, typename ContactAction>
    void process_group_message_operation(
        database& db, const Operation& po, const std::string& requester,
        Map& map, ProcessAction&& process_action, ContactAction&& contact_action
    ) {
        if (po.nonce != 0) {
            auto& idx = db.get_index<message_index>().indices().get<by_nonce>();
            auto itr = idx.find(std::make_tuple(po.from, po.to, po.nonce));

            if (itr == idx.end()) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("from", po.from)("to", po.to)("nonce", po.nonce));
            }

            process_action(*itr);
        } else if (po.from.size() && po.to.size() && po.from == requester) {
            if (!process_private_messages<by_outbox_account>(db, po, process_action, po.from, po.to)) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("from", po.from)("to", po.to)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        } else if (po.from.size() && po.to.size()) {
            if (!process_private_messages<by_inbox_account>(db, po, process_action, po.to, po.from)) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("from", po.from)("to", po.to)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        } else if (po.from.size()) {
            if (!process_private_messages<by_outbox>(db, po, process_action, po.from)) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("from", po.from)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        } else if (po.to.size()) {
            if (!process_private_messages<by_inbox>(db, po, process_action, po.to)) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("to", po.to)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        } else {
            if (!process_private_messages<by_inbox>(db, po, process_action, requester) &&
                !process_private_messages<by_outbox>(db, po, process_action, requester)
            ) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("requester", requester)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        }

        auto& contact_idx = db.get_index<contact_index>().indices().get<by_contact>();
        auto& size_idx = db.get_index<contact_size_index>().indices().get<by_owner>();

        for (const auto& stat_info: map) {
            const auto& owner = std::get<0>(stat_info.first);
            const auto& size = stat_info.second;
            auto contact_itr = contact_idx.find(stat_info.first);
            auto size_itr = size_idx.find(owner);

            FC_ASSERT(contact_idx.end() != contact_itr && size_idx.end() != size_itr, "Invalid size");

            if (!contact_action(*contact_itr, *size_itr, size)) {
                db.modify(*contact_itr, [&](auto& pco) {
                    pco.size -= size;
                });
                db.modify(*size_itr, [&](auto& pcso){
                    pcso.size -= size;
                });
            }
        }
    }

    template <typename Impl>
    void private_delete_message_evaluator<Impl>::do_apply(const private_delete_message_operation& pdm) {
        if (!impl_->is_tracked_account(pdm.from) &&
            !impl_->is_tracked_account(pdm.to) &&
            !impl_->is_tracked_account(pdm.requester)
        ) {
            return;
        }

        database& d = impl_->db_;
        auto now = d.head_block_time();
        fc::flat_map<std::tuple<account_name_type, account_name_type>, contact_size_info> stat_map;

        process_group_message_operation(
            d, pdm, pdm.requester, stat_map,
            /* process_action */
            [&](const message_object& m) -> bool {
                uint32_t unread_messages = 0;

                if (m.read_date == time_point_sec::min()) {
                    unread_messages = 1;
                }
                if (pdm.requester == pdm.to) {
                    // remove from inbox
                    if (m.inbox_create_date == time_point_sec::min()) {
                        return false;
                    }
                    auto& inbox_stat = stat_map[std::make_tuple(m.to, m.from)];
                    inbox_stat.unread_inbox_messages += unread_messages;
                    inbox_stat.total_inbox_messages++;
                } else {
                    // remove from outbox
                    if (m.outbox_create_date == time_point_sec::min()) {
                        return false;
                    }
                    auto& outbox_stat = stat_map[std::make_tuple(m.from, m.to)];
                    outbox_stat.unread_outbox_messages += unread_messages;
                    outbox_stat.total_outbox_messages++;
                }

                if (this->impl_->can_call_callbacks()) {
                    message_api_object ma(m);
                    ma.remove_date = now;

                    if (pdm.requester == pdm.to) {
                        this->impl_->call_callbacks(
                            callback_event_type::remove_inbox, m.from, m.to,
                            fc::variant(callback_message_event(
                                {callback_event_type::remove_inbox, ma})));
                    } else {
                        this->impl_->call_callbacks(
                            callback_event_type::remove_outbox, m.from, m.to,
                            fc::variant(callback_message_event(
                                {callback_event_type::remove_outbox, ma})));
                    }
                }

                if (m.remove_date == time_point_sec::min()) {
                    d.modify(m, [&](auto& m) {
                        m.remove_date = now;
                        if (pdm.requester == pdm.to) {
                            m.inbox_create_date = time_point_sec::min(); // remove message from find requests
                        } else {
                            m.outbox_create_date = time_point_sec::min(); // remove message from find requests
                        }
                    });
                } else {
                    d.remove(m);
                }
                return true;
            },
            /* contact_action */
            [&](const contact_object& co, const contact_size_object& so, const contact_size_info& size) -> bool {
                if (co.size != size || co.type != unknown) {
                    return false;
                }
                d.remove(co);
                if (so.size.total_contacts == 1) {
                    d.remove(so);
                } else {
                    d.modify(so, [&](auto& pcso) {
                        pcso.size.total_contacts--;
                        pcso.size -= size;
                    });
                }
                return true;
            }
        );
    }

    template <typename Impl>
    void private_mark_message_evaluator<Impl>::do_apply(const private_mark_message_operation& pmm) {
        if (!impl_->is_tracked_account(pmm.from) && !impl_->is_tracked_account(pmm.to)) {
            return;
        }

        database& d = impl_->db_;

        uint32_t total_marked_messages = 0;
        auto now = d.head_block_time();
        fc::flat_map<std::tuple<account_name_type, account_name_type>, contact_size_info> stat_map;

        process_group_message_operation(
            d, pmm, pmm.to, stat_map,
            /* process_action */
            [&](const message_object& m) -> bool {
                if (m.read_date != time_point_sec::min()) {
                    return true;
                }
                // only recipient can mark messages
                stat_map[std::make_tuple(m.to, m.from)].unread_inbox_messages++;
                // if sender hasn't yet removed the message
                if (m.remove_date == time_point_sec::min()) {
                    stat_map[std::make_tuple(m.from, m.to)].unread_outbox_messages++;
                }
                total_marked_messages++;

                d.modify(m, [&](message_object& m){
                    m.read_date = now;
                });

                if (this->impl_->can_call_callbacks()) {
                    this->impl_->call_callbacks(
                        callback_event_type::mark, m.from, m.to,
                        fc::variant(callback_message_event({callback_event_type::mark, message_api_object(m)})));
                }
                return true;
            },

            /* contact_action */
            [&](const contact_object&, const contact_size_object&, const contact_size_info&) -> bool {
                return false;
            }
        );

        GOLOS_CHECK_LOGIC(total_marked_messages > 0,
            logic_errors::no_unread_messages,
            "No unread messages in requested range");
    }

    template <typename Impl>
    void private_settings_evaluator<Impl>::do_apply(const private_settings_operation& ps) {
        if (!impl_->is_tracked_account(ps.owner)) {
            return;
        }

        database& d = impl_->db_;

        auto& idx = d.get_index<settings_index>().indices().get<by_owner>();
        auto itr = idx.find(ps.owner);

        auto set_settings = [&](settings_object& pso) {
            pso.owner = ps.owner;
            pso.ignore_messages_from_unknown_contact = ps.ignore_messages_from_unknown_contact;
        };

        if (idx.end() != itr) {
            d.modify(*itr, set_settings);
        } else {
            d.create<settings_object>(set_settings);
        }
    }

    template <typename Impl>
    void private_contact_evaluator<Impl>::do_apply(const private_contact_operation& pc) {
        if (!impl_->is_tracked_account(pc.owner) && !impl_->is_tracked_account(pc.contact)) {
            return;
        }

        database& d = impl_->db_;

        auto& contact_idx = d.get_index<contact_index>().indices().get<by_contact>();
        auto contact_itr = contact_idx.find(std::make_tuple(pc.owner, pc.contact));

        d.get_account(pc.contact);

        GOLOS_CHECK_LOGIC(contact_idx.end() != contact_itr || pc.type != unknown,
            logic_errors::add_unknown_contact,
            "Can't add unknown contact");

        std::string json_metadata(contact_itr->json_metadata.begin(), contact_itr->json_metadata.end());
        GOLOS_CHECK_LOGIC(contact_itr->type != pc.type || pc.json_metadata != json_metadata,
            logic_errors::contact_has_not_changed,
            "Contact hasn't changed");

        auto& owner_idx = d.get_index<contact_size_index>().indices().get<by_owner>();
        auto dst_itr = owner_idx.find(std::make_tuple(pc.owner, pc.type));

        if (contact_idx.end() != contact_itr) {
            auto src_itr = owner_idx.find(std::make_tuple(pc.owner, contact_itr->type));
            if (contact_itr->type != pc.type) {
                // last contact
                if (src_itr->size.total_contacts == 1) {
                    d.remove(*src_itr);
                } else {
                    d.modify(*src_itr, [&](auto& src) {
                        src.size.total_contacts--;
                        src.size -= contact_itr->size;
                    });
                }

                // has messages or type is not unknown
                if (!contact_itr->size.empty() || pc.type != unknown) {
                    auto modify_counters = [&](auto& dst) {
                        dst.size.total_contacts++;
                        dst.size += contact_itr->size;
                    };

                    if (owner_idx.end() == dst_itr) {
                        d.create<contact_size_object>([&](auto& dst) {
                            dst.owner = pc.owner;
                            dst.type = pc.type;
                            modify_counters(dst);
                        });
                    } else {
                        d.modify(*dst_itr, modify_counters);
                    }
                }
            }

            // contact is unknown and no messages
            if (pc.type == unknown && contact_itr->size.empty()) {
                d.remove(*contact_itr);
            } else {
                d.modify(*contact_itr, [&](auto& plo) {
                    plo.type = pc.type;
                    from_string(plo.json_metadata, pc.json_metadata);
                });
            }
        } else if (pc.type != unknown) {
            d.create<contact_object>([&](auto& plo){
                plo.owner = pc.owner;
                plo.contact = pc.contact;
                plo.type = pc.type;
                from_string(plo.json_metadata, pc.json_metadata);
            });

            contact_itr = contact_idx.find(std::make_tuple(pc.owner, pc.contact));

            if (owner_idx.end() == dst_itr) {
                d.create<contact_size_object>([&](auto& pcso) {
                    pcso.owner = pc.owner;
                    pcso.type = pc.type;
                    pcso.size.total_contacts = 1;
                });
            } else {
                d.modify(*dst_itr, [&](auto& pcso) {
                    pcso.size.total_contacts++;
                });
            }
        }

        if (this->impl_->can_call_callbacks()) {
            this->impl_->call_callbacks(
                callback_event_type::contact, pc.owner, pc.contact,
                fc::variant(callback_contact_event(
                    {callback_event_type::contact, contact_api_object(*contact_itr)})));
        }
    }

    private_message_plugin::private_message_plugin() = default;

    private_message_plugin::~private_message_plugin() = default;

    const std::string& private_message_plugin::name() {
        static std::string name = "private_message";
        return name;
    }

    void private_message_plugin::set_program_options(
        boost::program_options::options_description& cli,
        boost::program_options::options_description& cfg
    ) {
        cfg.add_options()
            ("pm-account-range",
             boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(),
             "Defines a range of accounts to private messages to/from as a json pair [\"from\",\"to\"] [from,to]")
            ("pm-account-list",
             boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(),
             "Defines a list of accounts to private messages to/from");
    }

    void private_message_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
        ilog("Intializing private message plugin");
        my = std::make_unique<private_message_plugin::private_message_plugin_impl>(*this);

        add_plugin_index<message_index>(my->db_);
        add_plugin_index<settings_index>(my->db_);
        add_plugin_index<contact_index>(my->db_);
        add_plugin_index<contact_size_index>(my->db_);

        using pairstring = std::pair<std::string, std::string>;
        LOAD_VALUE_SET(options, "pm-account-range", my->tracked_account_ranges_, pairstring);
        if (options.count("pm-account-list")) {
            auto list = options["pm-account-list"].as<std::vector<std::string>>();
            my->tracked_account_list_.insert(list.begin(), list.end());
        }
        JSON_RPC_REGISTER_API(name())
    }

    void private_message_plugin::plugin_startup() {
        ilog("Starting up private message plugin");
    }

    void private_message_plugin::plugin_shutdown() {
        ilog("Shuting down private message plugin");
    }

    bool private_message_plugin::private_message_plugin_impl::is_tracked_account(account_name_type name) const {
        if (tracked_account_ranges_.empty() && tracked_account_list_.empty()) {
            return true;
        }

        auto list_itr = tracked_account_list_.find(name);
        if (tracked_account_list_.end() != list_itr) {
            return true;
        }

        auto range_itr = tracked_account_ranges_.lower_bound(name);
        return tracked_account_ranges_.end() != range_itr && name >= range_itr->first && name <= range_itr->second;
    }

    // Api Defines

    DEFINE_API(private_message_plugin, get_inbox) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, to)
            (message_box_query, query)
        );

        GOLOS_CHECK_LIMIT_PARAM(query.limit, PRIVATE_DEFAULT_LIMIT);

        GOLOS_CHECK_PARAM(query.filter_accounts, {
            for (auto& itr : query.filter_accounts) {
                GOLOS_CHECK_VALUE(!query.select_accounts.count(itr),
                    "Can't filter and select accounts '${account}' at the same time",
                    ("account", itr));
            }
        });

        return my->db_.with_weak_read_lock([&]() {
            return my->get_message_box<by_inbox>(
                to, query,
                [&](const message_object& o) -> const account_name_type& {
                    return o.from;
                }
            );
        });
    }

    DEFINE_API(private_message_plugin, get_outbox) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, from)
            (message_box_query, query)
        );

        GOLOS_CHECK_LIMIT_PARAM(query.limit, PRIVATE_DEFAULT_LIMIT);

        GOLOS_CHECK_PARAM(query.filter_accounts, {
            for (auto& itr : query.filter_accounts) {
                GOLOS_CHECK_VALUE(!query.select_accounts.count(itr),
                    "Can't filter and select accounts '${account}' at the same time",
                    ("account", itr));
            }
        });

        return my->db_.with_weak_read_lock([&]() {
            return my->get_message_box<by_outbox>(
                from, query,
                [&](const message_object& o) -> const account_name_type& {
                    return o.to;
                });
        });
    }

    DEFINE_API(private_message_plugin, get_thread) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, from)
            (std::string, to)
            (message_thread_query, query)
        );

        GOLOS_CHECK_LIMIT_PARAM(query.limit, PRIVATE_DEFAULT_LIMIT);

        if (!query.limit) {
            query.limit = PRIVATE_DEFAULT_LIMIT;
        }

        if (query.newest_date == time_point_sec::min()) {
            query.newest_date = my->db_.head_block_time();
        }

        return my->db_.with_weak_read_lock([&]() {
            return my->get_thread(from, to, query);
        });
    }

    DEFINE_API(private_message_plugin, get_settings) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, owner)
        );

        return my->db_.with_weak_read_lock([&](){
            return my->get_settings(owner);
        });
    }

    DEFINE_API(private_message_plugin, get_contacts_size) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, owner)
        );

        return my->db_.with_weak_read_lock([&](){
            return my->get_contacts_size(owner);
        });
    }

    DEFINE_API(private_message_plugin, get_contact_info) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, owner)
            (std::string, contact)
        );

        return my->db_.with_weak_read_lock([&](){
            return my->get_contact_info(owner, contact);
        });
    }

    DEFINE_API(private_message_plugin, get_contacts) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, owner)
            (private_contact_type, type)
            (uint16_t, limit)
            (uint32_t, offset)
        );

        GOLOS_CHECK_LIMIT_PARAM(limit, 100);

        return my->db_.with_weak_read_lock([&](){
            return my->get_contacts(owner, type, limit, offset);
        });
    }

    DEFINE_API(private_message_plugin, set_callback) {
        PLUGIN_API_VALIDATE_ARGS(
            (callback_query, query)
        );

        GOLOS_CHECK_PARAM(query.filter_accounts, {
            for (auto& itr : query.filter_accounts) {
                GOLOS_CHECK_VALUE(!query.select_accounts.count(itr),
                    "Can't filter and select accounts '${account}' at the same time",
                    ("account", itr));
            }
        });

        GOLOS_CHECK_PARAM(query.filter_events, {
            for (auto& itr : query.filter_events) {
                GOLOS_CHECK_VALUE(!query.select_events.count(itr),
                    "Can't filter and select event '${event}' at the same time",
                    ("event", itr));
            }
        });

        json_rpc::msg_pack_transfer transfer(args);
        {
            std::lock_guard<std::mutex> lock(my->callbacks_mutex_);
            my->callbacks_.emplace_back(std::move(query), transfer.msg());
        };
        transfer.complete();
        return {};
    }

} } } // golos::plugins::private_message

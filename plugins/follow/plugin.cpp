#include <golos/plugins/follow/follow_objects.hpp>
#include <golos/plugins/follow/follow_operations.hpp>
#include <golos/plugins/follow/follow_evaluators.hpp>
#include <golos/protocol/config.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/generic_custom_operation_interpreter.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <memory>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/chain/index.hpp>
#include <golos/api/discussion_helper.hpp>

namespace golos {

template<>
std::string get_logic_error_namespace<golos::plugins::follow::logic_errors::error_type>() {
    return golos::plugins::follow::plugin::name();
}

} // namespace golos

namespace golos {
    namespace plugins {
        namespace follow {
            using namespace golos::protocol;
            using golos::chain::generic_custom_operation_interpreter;
            using golos::chain::custom_operation_interpreter;
            using golos::chain::operation_notification;
            using golos::chain::to_string;
            using golos::chain::account_index;
            using golos::chain::by_name;
            using golos::api::discussion_helper;

            void fill_account_reputation(
                const golos::chain::database& db,
                const account_name_type& account,
                fc::optional<share_type>& reputation
            ) {
                if (!db.has_index<follow::reputation_index>()) {
                    return;
                }

                auto& rep_idx = db.get_index<follow::reputation_index>().indices().get<follow::by_account>();
                auto itr = rep_idx.find(account);
                if (rep_idx.end() != itr) {
                    reputation = itr->reputation;
                } else {
                    reputation = 0;
                }
            }

            struct pre_operation_visitor {
                plugin& _plugin;
                golos::chain::database& db;

                pre_operation_visitor(plugin& plugin, golos::chain::database& db) : _plugin(plugin), db(db) {
                }

                typedef void result_type;

                template<typename T>
                void operator()(const T&) const {
                }

                void operator()(const vote_operation& op) const {
                    try {

                        const auto& c = db.get_comment(op.author, op.permlink);

                        if (db.calculate_discussion_payout_time(c) == fc::time_point_sec::maximum()) {
                            return;
                        }

                        const auto& cv_idx = db.get_index<comment_vote_index>().indices().get<by_comment_voter>();
                        auto cv = cv_idx.find(std::make_tuple(c.id, db.get_account(op.voter).id));

                        if (cv != cv_idx.end()) {
                            const auto& rep_idx = db.get_index<reputation_index>().indices().get<by_account>();
                            auto rep = rep_idx.find(op.author);

                            if (rep != rep_idx.end()) {
                                db.modify(*rep, [&](reputation_object& r) {
                                    r.reputation -= (cv->rshares >> 6); // Shift away precision from vests. It is noise
                                });
                            }
                        }
                    } catch (const fc::exception& e) {
                    }
                }

                void operator()(const delete_comment_operation& op) const {
                    try {
                        const auto* comment = db.find_comment(op.author, op.permlink);

                        if (comment == nullptr) {
                            return;
                        }
                        if (comment->parent_author.size()) {
                            return;
                        }

                        const auto& feed_idx = db.get_index<feed_index>().indices().get<by_comment>();
                        auto itr = feed_idx.lower_bound(comment->id);

                        while (itr != feed_idx.end() && itr->comment == comment->id) {
                            const auto& old_feed = *itr;
                            ++itr;
                            db.remove(old_feed);
                        }

                        const auto& blog_idx = db.get_index<blog_index>().indices().get<by_comment>();
                        auto blog_itr = blog_idx.lower_bound(comment->id);

                        while (blog_itr != blog_idx.end() && blog_itr->comment == comment->id) {
                            const auto& old_blog = *blog_itr;
                            ++blog_itr;
                            db.remove(old_blog);
                        }
                    } FC_CAPTURE_AND_RETHROW()
                }
            };

            struct post_operation_visitor {
                plugin& _plugin;
                database& db;

                post_operation_visitor(plugin& plugin, database& db) : _plugin(plugin), db(db) {
                }

                typedef void result_type;

                template<typename T>
                void operator()(const T&) const {
                }

                void operator()(const custom_json_operation& op) const {
                    try {
                        if (op.id == plugin::plugin_name) {
                            custom_json_operation new_cop;

                            new_cop.required_auths = op.required_auths;
                            new_cop.required_posting_auths = op.required_posting_auths;
                            new_cop.id = plugin::plugin_name;
                            follow_operation fop;

                            try {
                                fop = fc::json::from_string(op.json).as<follow_operation>();
                            } catch (const fc::exception&) {
                                return;
                            }

                            auto new_fop = follow_plugin_operation(fop);
                            new_cop.json = fc::json::to_string(new_fop);
                            std::shared_ptr<custom_operation_interpreter> eval = db.get_custom_json_evaluator(op.id);
                            eval->apply(new_cop);
                        }
                    } FC_CAPTURE_AND_RETHROW()
                }

                void operator()(const comment_operation& op) const {
                    try {
                        if (op.parent_author.size() > 0) {
                            return;
                        }

                        const auto& c = db.get_comment(op.author, op.permlink);

                        if (c.created != db.head_block_time()) {
                            return;
                        }

                        const auto& idx = db.get_index<follow_index>().indices().get<by_following_follower>();
                        const auto& comment_idx = db.get_index<feed_index>().indices().get<by_comment>();
                        auto itr = idx.find(op.author);

                        const auto& feed_idx = db.get_index<feed_index>().indices().get<by_feed>();

                        while (itr != idx.end() && itr->following == op.author) {
                            if (itr->what & (1 << blog)) {
                                uint32_t next_id = 0;
                                auto last_feed = feed_idx.lower_bound(itr->follower);

                                if (last_feed != feed_idx.end() && last_feed->account == itr->follower) {
                                    next_id = last_feed->account_feed_id + 1;
                                }

                                if (comment_idx.find(boost::make_tuple(c.id, itr->follower)) == comment_idx.end()) {
                                    db.create<feed_object>([&](feed_object& f) {
                                        f.account = itr->follower;
                                        f.comment = c.id;
                                        f.account_feed_id = next_id;
                                    });

                                    const auto& old_feed_idx = db.get_index<feed_index>().indices().get<by_old_feed>();
                                    auto old_feed = old_feed_idx.lower_bound(itr->follower);

                                    while (old_feed->account == itr->follower &&
                                           next_id - old_feed->account_feed_id > _plugin.max_feed_size()) {
                                        db.remove(*old_feed);
                                        old_feed = old_feed_idx.lower_bound(itr->follower);
                                    }
                                }
                            }

                            ++itr;
                        }

                        const auto& blog_idx = db.get_index<blog_index>().indices().get<by_blog>();
                        const auto& comment_blog_idx = db.get_index<blog_index>().indices().get<by_comment>();
                        auto last_blog = blog_idx.lower_bound(op.author);
                        uint32_t next_id = 0;

                        if (last_blog != blog_idx.end() && last_blog->account == op.author) {
                            next_id = last_blog->blog_feed_id + 1;
                        }

                        if (comment_blog_idx.find(boost::make_tuple(c.id, op.author)) == comment_blog_idx.end()) {
                            db.create<blog_object>([&](blog_object& b) {
                                b.account = op.author;
                                b.comment = c.id;
                                b.blog_feed_id = next_id;
                            });

                            const auto& old_blog_idx = db.get_index<blog_index>().indices().get<by_old_blog>();
                            auto old_blog = old_blog_idx.lower_bound(op.author);

                            while (old_blog->account == op.author &&
                                   next_id - old_blog->blog_feed_id > _plugin.max_feed_size()) {
                                db.remove(*old_blog);
                                old_blog = old_blog_idx.lower_bound(op.author);
                            }
                        }
                    } FC_LOG_AND_RETHROW()
                }

                void operator()(const vote_operation& op) const {
                    try {
                        const auto& comment = db.get_comment(op.author, op.permlink);

                        if (db.calculate_discussion_payout_time(comment) == fc::time_point_sec::maximum()) {
                            return;
                        }

                        const auto& cv_idx = db.get_index<comment_vote_index>().indices().get<by_comment_voter>();
                        auto cv = cv_idx.find(boost::make_tuple(comment.id, db.get_account(op.voter).id));

                        const auto& rep_idx = db.get_index<reputation_index>().indices().get<by_account>();
                        auto voter_rep = rep_idx.find(op.voter);
                        auto author_rep = rep_idx.find(op.author);

                        // Rules are a plugin, do not effect consensus, and are subject to change.
                        // Rule #1: Must have non-negative reputation to effect another user's reputation
                        if (voter_rep != rep_idx.end() && voter_rep->reputation < 0) {
                            return;
                        }

                        if (author_rep == rep_idx.end()) {
                            // Rule #2: If you are down voting another user, you must have more reputation than them to impact their reputation
                            // User rep is 0, so requires voter having positive rep
                            if (cv->rshares < 0 && !(voter_rep != rep_idx.end() && voter_rep->reputation > 0)) {
                                return;
                            }

                            db.create<reputation_object>([&](reputation_object& r) {
                                r.account = op.author;
                                r.reputation = (cv->rshares >> 6); // Shift away precision from vests. It is noise
                            });
                        } else {
                            // Rule #2: If you are down voting another user, you must have more reputation than them to impact their reputation
                            if (cv->rshares < 0 &&
                                !(voter_rep != rep_idx.end() && voter_rep->reputation > author_rep->reputation)) {
                                return;
                            }

                            db.modify(*author_rep, [&](reputation_object& r) {
                                r.reputation += (cv->rshares >> 6); // Shift away precision from vests. It is noise
                            });
                        }
                    } FC_CAPTURE_AND_RETHROW()
                }
            };

            inline void set_what(std::vector<follow_type>& what, uint16_t bitmask) {
                if (bitmask & 1 << blog) {
                    what.push_back(blog);
                }
                if (bitmask & 1 << ignore) {
                    what.push_back(ignore);
                }
            }

            struct plugin::impl final {
            public:
                impl() : database_(appbase::app().get_plugin<chain::plugin>().db()) {
                    helper = std::make_unique<discussion_helper>(
                        database_,
                        follow::fill_account_reputation,
                        nullptr,
                        golos::plugins::social_network::fill_comment_info
                    );
                }

                ~impl() {
                };

                void plugin_initialize(plugin& self) {
                    // Each plugin needs its own evaluator registry.
                    _custom_operation_interpreter = std::make_shared<
                            generic_custom_operation_interpreter<follow_plugin_operation>>(database());

                    // Add each operation evaluator to the registry
                    _custom_operation_interpreter->register_evaluator<follow_evaluator>(&self);
                    _custom_operation_interpreter->register_evaluator<reblog_evaluator>(&self);
                    _custom_operation_interpreter->register_evaluator<delete_reblog_evaluator>(&self);

                    // Add the registry to the database so the database can delegate custom ops to the plugin
                    database().set_custom_operation_interpreter(plugin_name, _custom_operation_interpreter);
                }

                golos::chain::database& database() {
                    return database_;
                }


                void pre_operation(const operation_notification& op_obj, plugin& self) {
                    try {
                        op_obj.op.visit(pre_operation_visitor(self, database()));
                    } catch (const fc::assert_exception&) {
                        if (database().is_producing()) {
                            throw;
                        }
                    }
                }

                void post_operation(const operation_notification& op_obj, plugin& self) {
                    try {
                        op_obj.op.visit(post_operation_visitor(self, database()));
                    } catch (fc::assert_exception) {
                        if (database().is_producing()) {
                            throw;
                        }
                    }
                }

                std::vector<follow_api_object> get_followers(
                        account_name_type account,
                        account_name_type start,
                        follow_type type,
                        uint32_t limit = 1000);

                std::vector<follow_api_object> get_following(
                        account_name_type account,
                        account_name_type start,
                        follow_type type,
                        uint32_t limit = 1000);

                std::vector<feed_entry> get_feed_entries(
                        account_name_type account,
                        uint32_t start_entry_id = 0,
                        uint32_t limit = 500);

                std::vector<blog_entry> get_blog_entries(
                        account_name_type account,
                        uint32_t start_entry_id = 0,
                        uint32_t limit = 500);

                std::vector<comment_feed_entry> get_feed(
                        account_name_type account,
                        uint32_t start_entry_id = 0,
                        uint32_t limit = 500);

                std::vector<comment_blog_entry> get_blog(
                        account_name_type account,
                        uint32_t start_entry_id = 0,
                        uint32_t limit = 500);

                std::vector<account_reputation> get_account_reputations(
                        std::vector < account_name_type > accounts);

                follow_count_api_obj get_follow_count(account_name_type start);

                std::vector<account_name_type> get_reblogged_by(
                        account_name_type author,
                        std::string permlink);

                blog_authors_r get_blog_authors(account_name_type );

                golos::chain::database& database_;

                uint32_t max_feed_size_ = 500;

                std::shared_ptr<generic_custom_operation_interpreter<
                        follow::follow_plugin_operation>> _custom_operation_interpreter;

                std::unique_ptr<discussion_helper> helper;
            };

            plugin::plugin() {

            }

            void plugin::set_program_options(boost::program_options::options_description& cli,
                                                    boost::program_options::options_description& cfg) {
                cli.add_options()
                    ("follow-max-feed-size", boost::program_options::value<uint32_t>()->default_value(500),
                        "Set the maximum size of cached feed for an account");
                cfg.add(cli);
            }

            void plugin::plugin_initialize(const boost::program_options::variables_map& options) {
                try {
                    ilog("Intializing follow plugin");
                    pimpl.reset(new impl());
                    auto& db = pimpl->database();
                    pimpl->plugin_initialize(*this);

                    db.pre_apply_operation.connect([&](operation_notification& o) {
                        pimpl->pre_operation(o, *this);
                    });
                    db.post_apply_operation.connect([&](const operation_notification& o) {
                        pimpl->post_operation(o, *this);
                    });
                    golos::chain::add_plugin_index<follow_index>(db);
                    golos::chain::add_plugin_index<feed_index>(db);
                    golos::chain::add_plugin_index<blog_index>(db);
                    golos::chain::add_plugin_index<reputation_index>(db);
                    golos::chain::add_plugin_index<follow_count_index>(db);
                    golos::chain::add_plugin_index<blog_author_stats_index>(db);

                    if (options.count("follow-max-feed-size")) {
                        uint32_t feed_size = options["follow-max-feed-size"].as<uint32_t>();
                        pimpl->max_feed_size_ = feed_size;
                    }

                    JSON_RPC_REGISTER_API ( name() ) ;
                } FC_CAPTURE_AND_RETHROW()
            }

            void plugin::plugin_startup() {
            }

            uint32_t plugin::max_feed_size() {
                return pimpl->max_feed_size_;
            }

            plugin::~plugin() {

            }


            std::vector<follow_api_object> plugin::impl::get_followers(
                    account_name_type account,
                    account_name_type start,
                    follow_type type,
                    uint32_t limit) {

                GOLOS_CHECK_LIMIT_PARAM(limit, 1000);
                std::vector<follow_api_object> result;
                result.reserve(limit);

                const auto& idx = database().get_index<follow_index>().indices().get<by_following_follower>();
                auto itr = idx.lower_bound(std::make_tuple(account, start));
                while (itr != idx.end() && result.size() < limit && itr->following == account) {
                    if (type == undefined || itr->what & (1 << type)) {
                        follow_api_object entry;
                        entry.follower = itr->follower;
                        entry.following = itr->following;
                        set_what(entry.what, itr->what);
                        result.push_back(entry);
                    }

                    ++itr;
                }

                return result;
            }

            std::vector<follow_api_object> plugin::impl::get_following(
                    account_name_type account,
                    account_name_type start,
                    follow_type type,
                    uint32_t limit) {

                GOLOS_CHECK_LIMIT_PARAM(limit, 100);
                std::vector<follow_api_object> result;
                const auto& idx = database().get_index<follow_index>().indices().get<by_follower_following>();
                auto itr = idx.lower_bound(std::make_tuple(account, start));
                while (itr != idx.end() && result.size() < limit && itr->follower == account) {
                    if (type == undefined || itr->what & (1 << type)) {
                        follow_api_object entry;
                        entry.follower = itr->follower;
                        entry.following = itr->following;
                        set_what(entry.what, itr->what);
                        result.push_back(entry);
                    }

                    ++itr;
                }

                return result;
            }

            follow_count_api_obj plugin::impl::get_follow_count(account_name_type acct) {
                follow_count_api_obj result;
                auto itr = database().find<follow_count_object, by_account>(acct);

                if (itr != nullptr) {
                    result = follow_count_api_obj(itr->account, itr->follower_count, itr->following_count, 1000);
                } else {
                    result.account = acct;
                }

                return result;
            }

            std::vector<feed_entry> plugin::impl::get_feed_entries(
                    account_name_type account,
                    uint32_t entry_id,
                    uint32_t limit) {
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                if (entry_id == 0) {
                    entry_id = ~0;
                }

                std::vector<feed_entry> result;
                result.reserve(limit);

                const auto& db = database();
                const auto& feed_idx = db.get_index<feed_index>().indices().get<by_feed>();
                auto itr = feed_idx.lower_bound(boost::make_tuple(account, entry_id));

                while (itr != feed_idx.end() && itr->account == account && result.size() < limit) {
                    const auto& comment = db.get(itr->comment);
                    feed_entry entry;
                    entry.author = comment.author;
                    entry.permlink = to_string(comment.permlink);
                    entry.entry_id = itr->account_feed_id;
                    if (itr->first_reblogged_by != account_name_type()) {
                        entry.reblog_by.reserve(itr->reblogged_by.size());
                        entry.reblog_entries.reserve(itr->reblogged_by.size());
                        for (const auto& a : itr->reblogged_by) {
                            entry.reblog_by.push_back(a);
                            const auto& blog_idx = db.get_index<blog_index>().indices().get<by_comment>();
                            auto blog_itr = blog_idx.find(std::make_tuple(itr->comment, a));
                            entry.reblog_entries.emplace_back(
                                a,
                                to_string(blog_itr->reblog_title),
                                to_string(blog_itr->reblog_body),
                                to_string(blog_itr->reblog_json_metadata)
                            );
                        }
                        entry.reblog_on = itr->first_reblogged_on;
                    }
                    result.push_back(entry);

                    ++itr;
                }

                return result;
            }

            std::vector<comment_feed_entry> plugin::impl::get_feed(
                    account_name_type account,
                    uint32_t entry_id,
                    uint32_t limit) {
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                if (entry_id == 0) {
                    entry_id = ~0;
                }

                std::vector<comment_feed_entry> result;
                result.reserve(limit);

                const auto& db = database();
                const auto& feed_idx = db.get_index<feed_index>().indices().get<by_feed>();
                auto itr = feed_idx.lower_bound(boost::make_tuple(account, entry_id));

                while (itr != feed_idx.end() && itr->account == account && result.size() < limit) {
                    const auto& comment = db.get(itr->comment);
                    comment_feed_entry entry;
                    entry.comment = helper->create_comment_api_object(comment);
                    entry.entry_id = itr->account_feed_id;
                    if (itr->first_reblogged_by != account_name_type()) {
                        entry.reblog_by.reserve(itr->reblogged_by.size());
                        entry.reblog_entries.reserve(itr->reblogged_by.size());
                        for (const auto& a : itr->reblogged_by) {
                            entry.reblog_by.push_back(a);
                            const auto& blog_idx = db.get_index<blog_index>().indices().get<by_comment>();
                            auto blog_itr = blog_idx.find(std::make_tuple(itr->comment, a));
                            entry.reblog_entries.emplace_back(
                                a,
                                to_string(blog_itr->reblog_title),
                                to_string(blog_itr->reblog_body),
                                to_string(blog_itr->reblog_json_metadata)
                            );
                        }
                        entry.reblog_on = itr->first_reblogged_on;
                    }
                    result.push_back(entry);

                    ++itr;
                }

                return result;
            }

            std::vector<blog_entry> plugin::impl::get_blog_entries(
                    account_name_type account,
                    uint32_t entry_id,
                    uint32_t limit) {
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                if (entry_id == 0) {
                    entry_id = ~0;
                }

                std::vector<blog_entry> result;
                result.reserve(limit);

                const auto& db = database();
                const auto& blog_idx = db.get_index<blog_index>().indices().get<by_blog>();
                auto itr = blog_idx.lower_bound(boost::make_tuple(account, entry_id));

                while (itr != blog_idx.end() && itr->account == account && result.size() < limit) {
                    const auto& comment = db.get(itr->comment);
                    blog_entry entry;
                    entry.author = comment.author;
                    entry.permlink = to_string(comment.permlink);
                    entry.blog = account;
                    entry.reblog_on = itr->reblogged_on;
                    entry.entry_id = itr->blog_feed_id;
                    entry.reblog_title = to_string(itr->reblog_title);
                    entry.reblog_body = to_string(itr->reblog_body);
                    entry.reblog_json_metadata = to_string(itr->reblog_json_metadata);

                    result.push_back(entry);

                    ++itr;
                }

                return result;
            }

            std::vector<comment_blog_entry> plugin::impl::get_blog(
                    account_name_type account,
                    uint32_t entry_id,
                    uint32_t limit) {
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                if (entry_id == 0) {
                    entry_id = ~0;
                }

                std::vector<comment_blog_entry> result;
                result.reserve(limit);

                const auto& db = database();
                const auto& blog_idx = db.get_index<blog_index>().indices().get<by_blog>();
                auto itr = blog_idx.lower_bound(boost::make_tuple(account, entry_id));

                while (itr != blog_idx.end() && itr->account == account && result.size() < limit) {
                    const auto& comment = db.get(itr->comment);
                    comment_blog_entry entry;
                    entry.comment = helper->create_comment_api_object(comment);
                    entry.blog = account;
                    entry.reblog_on = itr->reblogged_on;
                    entry.entry_id = itr->blog_feed_id;
                    entry.reblog_title = to_string(itr->reblog_title);
                    entry.reblog_body = to_string(itr->reblog_body);
                    entry.reblog_json_metadata = to_string(itr->reblog_json_metadata);

                    result.push_back(entry);

                    ++itr;
                }

                return result;
            }

            std::vector<account_reputation> plugin::impl::get_account_reputations(
                    std::vector < account_name_type > accounts
                ) {

                GOLOS_CHECK_PARAM(accounts,
                    GOLOS_CHECK_VALUE(accounts.size() <= 100, "Cannot retrieve more than 100 account reputations at a time."));

                const auto& idx = database().get_index<account_index>().indices().get<by_name>();

                size_t acc_count = accounts.size();

                std::vector<account_reputation> result;
                result.reserve(acc_count);

                for (size_t i = 0; i < acc_count; i++) {
                    account_reputation rep;
                    auto itr = idx.find(accounts[i]);

                    // checking the presence of account with such name in database
                    if (itr == idx.end()) {
                        rep.account = accounts[i];
                        rep.reputation = 0;
                        result.push_back(std::move(rep));
                        continue;
                    }

                    rep.account = itr->name;
                    fill_account_reputation(database(), itr->name, rep.reputation);
                    result.push_back(std::move(rep));
                }
                return result;
            }

            std::vector<account_name_type> plugin::impl::get_reblogged_by(
                    account_name_type author,
                    std::string permlink
            ) {
                auto& db = database();
                std::vector<account_name_type> result;
                const auto& post = db.get_comment(author, permlink);
                const auto& blog_idx = db.get_index<blog_index, by_comment>();
                auto itr = blog_idx.lower_bound(post.id);
                while (itr != blog_idx.end() && itr->comment == post.id && result.size() < 2000) {
                    result.push_back(itr->account);
                    ++itr;
                }
                return result;
            }

            blog_authors_r plugin::impl::get_blog_authors(account_name_type blog_account) {
                auto& db = database();
                blog_authors_r result;
                const auto& stats_idx = db.get_index<blog_author_stats_index, by_blogger_guest_count>();
                auto itr = stats_idx.lower_bound(boost::make_tuple(blog_account));
                while (itr != stats_idx.end() && itr->blogger == blog_account && result.size()) {
                    result.emplace_back(itr->guest, itr->count);
                    ++itr;
                }
                return result;
            }

            DEFINE_API(plugin, get_followers) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, following)
                    (account_name_type, start_follower)
                    (follow_type,       type)
                    (uint32_t,          limit)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_followers(following, start_follower, type, limit);
                });
            }

            DEFINE_API(plugin, get_following) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, follower)
                    (account_name_type, start_following)
                    (follow_type,       type)
                    (uint32_t,          limit)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_following(follower, start_following, type, limit);
                });
            }

            DEFINE_API(plugin, get_follow_count) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, account)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_follow_count(account);
                });
            }

            DEFINE_API(plugin, get_feed_entries){
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, account)
                    (uint32_t,          entry_id)
                    (uint32_t,          limit)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_feed_entries(account, entry_id, limit);
                });
            }

            DEFINE_API(plugin, get_feed) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, account)
                    (uint32_t,          entry_id)
                    (uint32_t,          limit)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_feed(account, entry_id, limit);
                });
            }

            DEFINE_API(plugin, get_blog_entries) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, account)
                    (uint32_t,          entry_id)
                    (uint32_t,          limit)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_blog_entries(account, entry_id, limit);
                });
            }

            DEFINE_API(plugin, get_blog) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, account)
                    (uint32_t,          entry_id)
                    (uint32_t,          limit)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_blog(account, entry_id, limit);
                });
            }

            DEFINE_API(plugin, get_account_reputations) {
                PLUGIN_API_VALIDATE_ARGS(
                    (std::vector<account_name_type>, accounts)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_account_reputations( accounts );
                });
            }

            DEFINE_API(plugin, get_reblogged_by) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, author)
                    (std::string,       permlink)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_reblogged_by(author, permlink);
                });
            }

            DEFINE_API(plugin, get_blog_authors) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, account)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_blog_authors(account);
                });
            }
        }
    }
} // golos::follow

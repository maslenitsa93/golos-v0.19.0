#pragma once

#include <golos/protocol/base.hpp>
#include <golos/protocol/operation_util.hpp>

namespace golos { namespace plugins { namespace private_message {
    using namespace golos::protocol;

    struct private_message_operation: public base_operation {
        account_name_type from;
        account_name_type to;
        uint64_t nonce;
        public_key_type from_memo_key;
        public_key_type to_memo_key;
        uint32_t checksum;
        bool update = false;
        std::vector<char> encrypted_message;

        void validate() const;
        void get_required_posting_authorities(flat_set<account_name_type>& a) const;
    };

    struct private_delete_message_operation: public base_operation {
        account_name_type requester;
        account_name_type from;
        account_name_type to;
        uint64_t nonce = 0;
        time_point_sec start_date;
        time_point_sec stop_date;

        void validate() const;
        void get_required_posting_authorities(flat_set<account_name_type>& a) const;
    };

    struct private_mark_message_operation: public base_operation {
        account_name_type from;
        account_name_type to;
        uint64_t nonce = 0;
        time_point_sec start_date;
        time_point_sec stop_date;

        void validate() const;
        void get_required_posting_authorities(flat_set<account_name_type>& a) const;
    };

    struct private_settings_operation: public base_operation {
        account_name_type owner;
        bool ignore_messages_from_unknown_contact = false;

        void validate() const;
        void get_required_posting_authorities(flat_set<account_name_type>& a) const;
    };

    /**
     * Types of contacts
     */
    enum private_contact_type: uint8_t {
        unknown = 1,
        pinned = 2,
        ignored = 3,
    };

    constexpr auto private_contact_type_size = static_cast<private_contact_type>(ignored + 1);

    struct private_contact_operation: public base_operation {
        account_name_type owner;
        account_name_type contact;
        private_contact_type type = pinned;
        std::string json_metadata;

        void validate() const;
        void get_required_posting_authorities(flat_set<account_name_type>& a) const;
    };

    using private_message_plugin_operation = fc::static_variant<
        private_message_operation,
        private_delete_message_operation,
        private_mark_message_operation,
        private_settings_operation,
        private_contact_operation>;

} } } // golos::plugins::private_message

FC_REFLECT(
    (golos::plugins::private_message::private_message_operation),
    (from)(to)(nonce)(from_memo_key)(to_memo_key)(checksum)(update)(encrypted_message))

FC_REFLECT(
    (golos::plugins::private_message::private_delete_message_operation),
    (requester)(from)(to)(nonce)(start_date)(stop_date))

FC_REFLECT(
    (golos::plugins::private_message::private_mark_message_operation),
    (from)(to)(nonce)(start_date)(stop_date))

FC_REFLECT(
    (golos::plugins::private_message::private_settings_operation),
    (owner)(ignore_messages_from_unknown_contact))

FC_REFLECT_ENUM(
    golos::plugins::private_message::private_contact_type,
    (unknown)(pinned)(ignored))

FC_REFLECT(
    (golos::plugins::private_message::private_contact_operation),
    (owner)(contact)(type)(json_metadata))

FC_REFLECT_TYPENAME((golos::plugins::private_message::private_message_plugin_operation))

DECLARE_OPERATION_TYPE(golos::plugins::private_message::private_message_plugin_operation)
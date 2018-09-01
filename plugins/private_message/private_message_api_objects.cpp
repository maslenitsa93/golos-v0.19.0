#include <golos/plugins/private_message/private_message_api_objects.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>

namespace golos { namespace plugins { namespace private_message {

    message_api_object::message_api_object(const message_object& o)
        : from(o.from),
          to(o.to),
          nonce(o.nonce),
          from_memo_key(o.from_memo_key),
          to_memo_key(o.to_memo_key),
          checksum(o.checksum),
          encrypted_message(o.encrypted_message.begin(), o.encrypted_message.end()),
          create_date(std::max(o.inbox_create_date, o.outbox_create_date)),
          receive_date(o.receive_date),
          read_date(o.read_date),
          remove_date(o.remove_date) {
    }

    message_api_object::message_api_object() = default;


    settings_api_object::settings_api_object(const settings_object& o)
        : ignore_messages_from_unknown_contact(o.ignore_messages_from_unknown_contact) {
    }

    settings_api_object::settings_api_object() = default;


    contact_api_object::contact_api_object(const contact_object& o)
        : owner(o.owner),
          contact(o.contact),
          json_metadata(o.json_metadata.begin(), o.json_metadata.end()),
          local_type(o.type),
          size(o.size) {
    }

    contact_api_object::contact_api_object() = default;

} } } // golos::plugins::private_message



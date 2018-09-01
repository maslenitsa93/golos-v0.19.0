#pragma once

#include <golos/chain/database.hpp>

namespace golos { namespace api {

    struct reblog_entry {
        account_name_type author;
        std::string title;
        std::string body;
        std::string json_metadata;

        reblog_entry() = default;

        reblog_entry(const account_name_type& author_, const std::string& title_, const std::string& body_,
                const std::string& json_metadata_)
                : author(author_), title(title_), body(body_), json_metadata(json_metadata_) {
        }
    };

} } // golos::api

FC_REFLECT((golos::api::reblog_entry), (author)(title)(body)(json_metadata));
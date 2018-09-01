#include <golos/plugins/follow/follow_operations.hpp>
#include <golos/protocol/operation_util_impl.hpp>

namespace golos {
    namespace plugins {
        namespace follow {

            /// TODO: after the hardfork, we can rename this method validate_permlink because it is strictily less restrictive than before
            ///  Issue #56 contains the justificiation for allowing any UTF-8 string to serve as a permlink, content will be grouped by tags
            ///  going forward.
            inline void validate_permlink(const string &permlink) {
                GOLOS_CHECK_VALUE(permlink.size() < STEEMIT_MAX_PERMLINK_LENGTH, "permlink is too long");
                GOLOS_CHECK_VALUE(fc::is_utf8(permlink), "permlink not formatted in UTF8");
            }

            void follow_operation::validate() const {
                GOLOS_CHECK_LOGIC(follower != following,
                        logic_errors::cannot_follow_yourself,
                        "You cannot follow yourself");
            }

            void reblog_operation::validate() const {
                GOLOS_CHECK_LOGIC(account != author, 
                        logic_errors::cannot_reblog_own_content,
                        "You cannot reblog your own content");

                GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));

                if (title.size() > 0 || body.size() > 0 || json_metadata.size() > 0) {
                    GOLOS_CHECK_PARAM(title, {
                        GOLOS_CHECK_VALUE(title.size() < 256, "Title larger than size limit");
                        GOLOS_CHECK_VALUE(fc::is_utf8(title), "Title not formatted in UTF8");
                    });

                    GOLOS_CHECK_PARAM(body, {
                        GOLOS_CHECK_VALUE(body.size() > 0, "Body is empty but Title or JSON Metadata is set");
                        GOLOS_CHECK_VALUE(fc::is_utf8(body), "Body not formatted in UTF8");
                    });

                    if (json_metadata.size() > 0) {
                        GOLOS_CHECK_PARAM(json_metadata, {
                            GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
                        });
                    }
                }
            }

            void delete_reblog_operation::validate() const {
                GOLOS_CHECK_LOGIC(account != author, 
                        logic_errors::cannot_delete_reblog_of_own_content,
                        "You cannot delete reblog of your own content");
                GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
            }

        }
    }
} //golos::follow

DEFINE_OPERATION_TYPE(golos::plugins::follow::follow_plugin_operation)

#pragma once

#include "tags_object.hpp"
#include <golos/chain/comment_object.hpp>
#include <golos/chain/account_object.hpp>
#include <boost/algorithm/string.hpp>


namespace golos { namespace plugins { namespace tags {
    using golos::api::discussion_helper;

    comment_metadata get_metadata(const std::string& json, std::size_t tags_number, std::size_t tag_max_length);

    struct comment_date { time_point_sec active; time_point_sec last_update; };

    struct operation_visitor {
        operation_visitor(database& db, std::size_t tags_number, std::size_t tag_max_length);
        using result_type = void;

        database& db_;
        std::size_t tags_number_;
        std::size_t tag_max_length_;

        void remove_stats(const tag_object& tag) const;

        void add_stats(const tag_object& tag) const;

        void remove_tag(const tag_object& tag) const;

        const tag_stats_object& get_stats(const tag_object&) const;

        comment_date get_comment_last_update(const comment_object& comment) const;

        void update_tag(const tag_object&, const comment_object&, double hot, double trending) const;

        void create_tag(const std::string&, const tag_type, const comment_object&, double hot, double trending) const;

        /**
         * https://medium.com/hacking-and-gonzo/how-reddit-ranking-algorithms-work-ef111e33d0d9#.lcbj6auuw
         */
        template<int64_t S, int32_t T>
        double calculate_score(const share_type& score, const time_point_sec& created) const {
            /// new algorithm
            auto mod_score = score.value / S;

            /// reddit algorithm
            double order = log10(std::max<int64_t>(std::abs(mod_score), 1));
            int sign = 0;

            if (mod_score > 0) {
                sign = 1;
            } else if (mod_score < 0) {
                sign = -1;
            }

            return sign * order + double(created.sec_since_epoch()) / double(T);
        }

        double calculate_hot(const share_type& score, const time_point_sec& created) const;

        double calculate_trending(const share_type& score, const time_point_sec& created) const;

        /** finds tags that have been added or removed or updated */
        void create_update_tags(const account_name_type& author, const std::string& permlink) const;
        void update_tags(const account_name_type& author, const std::string& permlink) const;
        void remove_tags(const account_name_type& author, const std::string& permlink) const;

        void operator()(const comment_operation& op) const;

        void operator()(const transfer_operation& op) const;

        void operator()(const vote_operation& op) const;

        void operator()(const delete_comment_operation& op) const;

        void operator()(const comment_reward_operation& op) const;

        void operator()(const comment_payout_update_operation& op) const;

        template<typename Op>
        void operator()(Op&&) const {
        } /// ignore all other ops
    };

} } } // golos::plugins::tags

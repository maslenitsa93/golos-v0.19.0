#pragma once

#include <golos/plugins/account_history/plugin.hpp>
#include <golos/plugins/database_api/forward.hpp>
#include <golos/plugins/database_api/plugin.hpp>
#include <golos/plugins/database_api/state.hpp>
#include <golos/plugins/follow/follow_api_object.hpp>
#include <golos/plugins/follow/plugin.hpp>
#include <golos/plugins/market_history/market_history_objects.hpp>
#include <golos/plugins/network_broadcast_api/network_broadcast_api_plugin.hpp>
#include <golos/plugins/operation_history/applied_operation.hpp>
#include <golos/plugins/private_message/private_message_api_objects.hpp>
#include <golos/plugins/social_network/social_network.hpp>
#include <golos/plugins/tags/tag_api_object.hpp>
#include <golos/plugins/tags/discussion_query.hpp>
#include <golos/plugins/witness_api/plugin.hpp>

#include <golos/api/account_api_object.hpp>
#include <golos/api/account_vote.hpp>
#include <golos/api/discussion.hpp>
#include <golos/api/vote_state.hpp>

#include <fc/api.hpp>

namespace golos { namespace wallet {

using std::vector;
using fc::variant;
using fc::optional;

using namespace chain;
using namespace plugins;
using namespace plugins::database_api;
using namespace plugins::follow;
using namespace plugins::social_network;
using namespace plugins::tags;
using namespace plugins::market_history;
using namespace plugins::network_broadcast_api;
using namespace plugins::private_message;
using namespace plugins::witness_api;
using namespace plugins::account_history;
using namespace golos::api;

/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to database_api plugin on remote node.
 */
struct remote_database_api {
    optional< database_api::signed_block > get_block( uint32_t );
    optional< block_header > get_block_header( uint32_t );
    fc::variant_object get_config();
    database_api::dynamic_global_property_object get_dynamic_global_properties();
    chain_properties get_chain_properties();
    hardfork_version get_hardfork_version();
    database_api::scheduled_hardfork get_next_scheduled_hardfork();
    vector< optional< golos::api::account_api_object > > lookup_account_names( vector< account_name_type > );
    vector< account_name_type > lookup_accounts( account_name_type, uint32_t );

    uint64_t get_account_count();
    vector< database_api::owner_authority_history_api_object > get_owner_history( account_name_type );
    optional< database_api::account_recovery_request_api_object > get_recovery_request( account_name_type );
    optional< database_api::escrow_api_object > get_escrow( account_name_type, uint32_t );
    vector< database_api::withdraw_vesting_route_api_object > get_withdraw_routes( account_name_type, database_api::withdraw_route_type );
    optional< account_bandwidth_api_object > get_account_bandwidth( account_name_type, bandwidth_type);
    vector< database_api::savings_withdraw_api_object > get_savings_withdraw_from( account_name_type );
    vector< database_api::savings_withdraw_api_object > get_savings_withdraw_to( account_name_type );
    vector< database_api::convert_request_api_object > get_conversion_requests( account_name_type );
    string get_transaction_hex( signed_transaction );
    set< public_key_type > get_required_signatures( signed_transaction, flat_set< public_key_type > );
    set< public_key_type > get_potential_signatures( signed_transaction );
    bool verify_authority( signed_transaction );
    bool verify_account_authority( string, flat_set< public_key_type > );
    vector< golos::api::account_api_object > get_accounts( vector< account_name_type > );
    database_api::database_info get_database_info();
    std::vector<proposal_api_object> get_proposed_transactions(account_name_type, uint32_t, uint32_t);
};

/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to operation_history plugin on remote node.
 */
struct remote_operation_history {
    vector< golos::plugins::operation_history::applied_operation > get_ops_in_block( uint32_t, bool only_virtual = true );
    annotated_signed_transaction get_transaction( transaction_id_type );
};

/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to operation_history plugin on remote node.
 */
struct remote_account_history {
    history_operations get_account_history(account_name_type, uint64_t, uint32_t, account_history_query);
};

/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to social_network plugin on remote node.
 */
struct remote_social_network {
    vector< tag_api_object > get_trending_tags( string, uint32_t );

    vector< tag_count_object > get_tags_used_by_author( account_name_type );

    vector< vote_state > get_active_votes( account_name_type, string );
    vector< account_vote > get_account_votes( account_name_type );

    discussion get_content( account_name_type, string );
    vector< discussion > get_content_replies( account_name_type, string );

    vector< discussion > get_discussions_by_payout( discussion_query );
    vector< discussion > get_discussions_by_trending( discussion_query );
    vector< discussion > get_discussions_by_created( discussion_query );
    vector< discussion > get_discussions_by_active( discussion_query );
    vector< discussion > get_discussions_by_cashout( discussion_query );
    vector< discussion > get_discussions_by_votes( discussion_query );
    vector< discussion > get_discussions_by_children( discussion_query );
    vector< discussion > get_discussions_by_hot( discussion_query );
    vector< discussion > get_discussions_by_feed( discussion_query );
    vector< discussion > get_discussions_by_blog( discussion_query );
    vector< discussion > get_discussions_by_comments( discussion_query );
    vector< discussion > get_discussions_by_promoted( discussion_query );
    vector< discussion > get_discussions_by_author_before_date( discussion_query );

    vector< discussion > get_replies_by_last_update( discussion_query );
};

/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to network_broadcast_api plugin on remote node.
 */
struct remote_network_broadcast_api {
    void broadcast_transaction( signed_transaction );
    broadcast_transaction_synchronous_t broadcast_transaction_synchronous( signed_transaction );
    void broadcast_block( signed_block );
};

/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to follow plugin on remote node.
 */
struct remote_follow {
    vector< follow_api_object > get_followers( account_name_type, account_name_type, follow_type, uint32_t );
    vector< follow_api_object > get_following( account_name_type, account_name_type, follow_type, uint32_t );
    get_follow_count_return get_follow_count( account_name_type );
    vector< feed_entry > get_feed_entries( account_name_type, uint32_t, uint32_t );
    vector< comment_feed_entry > get_feed( account_name_type, uint32_t, uint32_t );
    vector< blog_entry > get_blog_entries( account_name_type, uint32_t, uint32_t );
    vector< comment_blog_entry > get_blog( account_name_type, uint32_t, uint32_t );
    vector< account_reputation > get_account_reputations( account_name_type, uint32_t );
    vector< account_name_type > get_reblogged_by( account_name_type, string );
    vector< reblog_count > get_blog_authors( account_name_type );
};


/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to market_history plugin on remote node.
 */
struct remote_market_history {
   //database_api::get_version_return get_version();
    // TODO This method is from tolstoy_api
   //database_api::reward_fund_apit_object get_reward_fund( string );
   //vector< account_id_type > get_account_references( account_id_type account_id );

   market_ticker get_ticker();
   market_volume get_volume();
   order_book get_order_book(uint32_t limit);
   vector<limit_order> get_open_orders(string accountname);
   vector<market_trade> get_trade_history(time_point_sec start, time_point_sec end, uint32_t limit);
   vector<market_trade> get_recent_trades(uint32_t limit);
   vector<bucket_object> get_market_history(uint32_t bucket_seconds, time_point_sec start, time_point_sec end);
   flat_set<uint32_t> get_market_history_buckets();
};

/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to market_history plugin on remote node.
 */
struct remote_private_message {
    vector <message_api_object> get_inbox(const std::string& to, const message_box_query&) const;
    vector <message_api_object> get_outbox(const std::string& from, const message_box_query&) const;
    vector <message_api_object> get_thread(const std::string& from, const std::string& to, const message_thread_query&) const;
    settings_api_object get_settings(const std::string& owner) const;
    contacts_size_api_object get_contacts_size(const std::string& owner) const;
    contact_api_object get_contact_info(const std::string& owner, const std::string& contact) const;
    std::vector<contact_api_object> get_contacts(const std::string& owner, private_contact_type, uint16_t limit, uint32_t offset) const;
};

/**
 * This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
 * Class is used by wallet to send formatted API calls to account_by_key plugin on remote node.
 */
 struct remote_account_by_key {
     vector< vector< account_name_type > > get_key_references( vector< public_key_type > );
 };

/**
* This is a dummy class exists only to provide method signature information to fc::api, not to execute calls.
* Class is used by wallet to send formatted API calls to witness_api plugin on remote node.
*/
struct remote_witness_api {
    price get_current_median_history_price();
    feed_history_api_object get_feed_history();
    vector< account_name_type > get_miner_queue();
    vector< account_name_type > get_active_witnesses();
    golos::chain::witness_schedule_object get_witness_schedule();
    vector< optional< witness_api::witness_api_object > > get_witnesses( vector< witness_id_type > );
    vector< witness_api::witness_api_object > get_witnesses_by_vote( account_name_type, uint32_t );
    optional< witness_api::witness_api_object > get_witness_by_account( account_name_type );
    vector< account_name_type > lookup_witness_accounts( string, uint32_t );
    uint64_t get_witness_count();
};

} }

/**
 * Declaration of remote API formatter to database_api plugin on remote node
 */
FC_API( golos::wallet::remote_database_api,
        (get_block)
        (get_block_header)
        (get_config)
        (get_dynamic_global_properties)
        (get_chain_properties)
        (get_hardfork_version)
        (get_next_scheduled_hardfork)
        (lookup_account_names)
        (lookup_accounts)
        (get_account_count)
        (get_owner_history)
        (get_recovery_request)
        (get_escrow)
        (get_withdraw_routes)
        (get_account_bandwidth)
        (get_savings_withdraw_from)
        (get_savings_withdraw_to)
        (get_conversion_requests)
        (get_transaction_hex)
        (get_required_signatures)
        (get_potential_signatures)
        (verify_authority)
        (verify_account_authority)
        (get_accounts)
        (get_database_info)
        (get_proposed_transactions)
)

/**
 * Declaration of remote API formatter to operation_history plugin on remote node
 */
FC_API( golos::wallet::remote_operation_history,
        (get_ops_in_block)
        (get_transaction)
)

/**
 * Declaration of remote API formatter to account_history plugin on remote node
 */
FC_API( golos::wallet::remote_account_history,
        (get_account_history)
)

/**
 * Declaration of remote API formatter to social_network plugin on remote node
 */
FC_API( golos::wallet::remote_social_network,
        (get_trending_tags)
        (get_tags_used_by_author)
        (get_active_votes)
        (get_account_votes)
        (get_content)
        (get_content_replies)
        (get_discussions_by_payout)
        (get_discussions_by_trending)
        (get_discussions_by_created)
        (get_discussions_by_active)
        (get_discussions_by_cashout)
        (get_discussions_by_votes)
        (get_discussions_by_children)
        (get_discussions_by_hot)
        (get_discussions_by_feed)
        (get_discussions_by_blog)
        (get_discussions_by_comments)
        (get_discussions_by_promoted)
        (get_discussions_by_author_before_date)
        (get_replies_by_last_update)
)

/**
 * Declaration of remote API formatter to network_broadcast_api plugin on remote node
 */
FC_API( golos::wallet::remote_network_broadcast_api,
        (broadcast_transaction)
        (broadcast_transaction_synchronous)
        (broadcast_block)
)

/**
 * Declaration of remote API formatter to follow plugin on remote node
 */
FC_API( golos::wallet::remote_follow,
        (get_followers)
        (get_following)
        (get_follow_count)
        (get_feed_entries)
        (get_feed)
        (get_blog_entries)
        (get_blog)
        (get_account_reputations)
        (get_reblogged_by)
        (get_blog_authors)
)

/**
 * Declaration of remote API formatter to follow plugin on remote node
 */
FC_API( golos::wallet::remote_market_history,
        (get_ticker)
        (get_volume)
        (get_order_book)
        (get_open_orders)
        (get_trade_history)
        (get_recent_trades)
        (get_market_history)
        (get_market_history_buckets)
)

/**
 * Declaration of remote API formatter to private message plugin on remote node
 */
FC_API( golos::wallet::remote_private_message,
        (get_inbox)
        (get_outbox)
        (get_thread)
        (get_settings)
        (get_contacts)
        (get_contacts_size)
        (get_contact_info)
)

/**
 * Declaration of remote API formatter to account by key plugin on remote node
 */
FC_API( golos::wallet::remote_account_by_key,
        (get_key_references)
)

/**
 * Declaration of remote API formatter to witness_api plugin on remote node
 */
FC_API( golos::wallet::remote_witness_api,
        (get_current_median_history_price)
        (get_feed_history)
        (get_miner_queue)
        (get_active_witnesses)
        (get_witness_schedule)
        (get_witnesses)
        (get_witnesses_by_vote)
        (get_witness_count)
        (get_witness_by_account)
        (lookup_witness_accounts)
)

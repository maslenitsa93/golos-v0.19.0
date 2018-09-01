#pragma once

#include <golos/protocol/exceptions.hpp>

#define STEEMIT_TRY_NOTIFY(signal, ...)                                     \
   try                                                                        \
   {                                                                          \
      signal( __VA_ARGS__ );                                                  \
   }                                                                          \
   catch( const golos::chain::plugin_exception& e )                         \
   {                                                                          \
      elog( "Caught plugin exception: ${e}", ("e", e.to_detail_string() ) );  \
      throw;                                                                  \
   }                                                                          \
   catch( const fc::exception& e )                                            \
   {                                                                          \
      elog( "Caught exception in plugin: ${e}", ("e", e.to_detail_string() ) ); \
   }                                                                          \
   catch( ... )                                                               \
   {                                                                          \
      wlog( "Caught unexpected exception in plugin" );                        \
   }

namespace golos {
    namespace chain {

        FC_DECLARE_EXCEPTION(chain_exception, 4000000, "blockchain exception")

        FC_DECLARE_DERIVED_EXCEPTION(undo_database_exception, golos::chain::chain_exception, 4070000, "undo database exception")

        FC_DECLARE_DERIVED_EXCEPTION(unlinkable_block_exception, golos::chain::chain_exception, 4080000, "unlinkable block")

        FC_DECLARE_DERIVED_EXCEPTION(unknown_hardfork_exception, golos::chain::chain_exception, 4090000, "chain attempted to apply unknown hardfork")

        FC_DECLARE_DERIVED_EXCEPTION(plugin_exception, golos::chain::chain_exception, 4100000, "plugin exception")

        FC_DECLARE_DERIVED_EXCEPTION(block_log_exception, golos::chain::chain_exception, 4110000, "block log exception")

        FC_DECLARE_DERIVED_EXCEPTION(pop_empty_chain, golos::chain::undo_database_exception, 4070001, "there are no blocks to pop")

        FC_DECLARE_DERIVED_EXCEPTION(database_revision_exception, golos::chain::chain_exception, 4120000, "database revision exception")

        FC_DECLARE_DERIVED_EXCEPTION(database_signal_exception, golos::chain::chain_exception, 4130000, "database signal exception")

    }
} // golos::chain


#pragma once

#include <fc/exception/exception.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos {
    namespace chain {


    }
} // golos::chain

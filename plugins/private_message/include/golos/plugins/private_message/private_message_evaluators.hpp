#pragma once

#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/chain/evaluator.hpp>

namespace golos { namespace chain {
    class database;
} } // golos::chain

namespace golos { namespace plugins { namespace private_message {

    class private_message_plugin;

    using golos::chain::evaluator_impl;
    using golos::chain::database;

    template <typename Impl>
    class private_message_evaluator final:
        public evaluator_impl<private_message_evaluator<Impl>, private_message_plugin_operation>
    {
    public:
        using operation_type = private_message_operation;

        private_message_evaluator(database& db, Impl* impl)
            : evaluator_impl<private_message_evaluator<Impl>, private_message_plugin_operation>(db),
              impl_(impl) {
        }

        void do_apply(const private_message_operation& o);

        Impl* impl_;
    };

    template <typename Impl>
    class private_delete_message_evaluator final:
        public evaluator_impl<private_delete_message_evaluator<Impl>, private_message_plugin_operation>
    {
    public:
        using operation_type = private_delete_message_operation;

        private_delete_message_evaluator(database& db, Impl* impl)
            : evaluator_impl<private_delete_message_evaluator<Impl>, private_message_plugin_operation>(db),
              impl_(impl) {
        }

        void do_apply(const private_delete_message_operation& o);

        Impl* impl_;
    };

    template <typename Impl>
    class private_mark_message_evaluator final:
        public evaluator_impl<private_mark_message_evaluator<Impl>, private_message_plugin_operation>
    {
    public:
        using operation_type = private_mark_message_operation;

        private_mark_message_evaluator(database& db, Impl* impl)
            : evaluator_impl<private_mark_message_evaluator<Impl>, private_message_plugin_operation>(db),
              impl_(impl) {
        }

        void do_apply(const private_mark_message_operation& o);

        Impl* impl_;
    };

    template <typename Impl>
    class private_settings_evaluator final:
        public evaluator_impl<private_settings_evaluator<Impl>, private_message_plugin_operation>
    {
    public:
        using operation_type = private_settings_operation;

        private_settings_evaluator(database& db, Impl* impl)
            : evaluator_impl<private_settings_evaluator<Impl>, private_message_plugin_operation>(db),
              impl_(impl) {
        }

        void do_apply(const private_settings_operation& o);

        Impl* impl_;
    };

    template <typename Impl>
    class private_contact_evaluator final:
        public evaluator_impl<private_contact_evaluator<Impl>, private_message_plugin_operation>
    {
    public:
        using operation_type = private_contact_operation;

        private_contact_evaluator(database& db, Impl* impl)
            : evaluator_impl<private_contact_evaluator<Impl>, private_message_plugin_operation>(db),
              impl_(impl) {
        }

        void do_apply(const private_contact_operation& o);

        Impl* impl_;
    };

} } }

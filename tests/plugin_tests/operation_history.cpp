#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"

#include <string>
#include <cstdint>

using golos::chain::add_operations_database_fixture;
using golos::plugins::operation_history::applied_operation;
using golos::plugins::json_rpc::msg_pack;
using golos::protocol::account_create_operation;

static const std::string OPERATIONS = "account_create_operation,delete_comment_operation,vote,comment";

struct operation_visitor {
    using result_type = std::string;
    template<class T>
    std::string operator()(const T&) const {
        return std::string(fc::get_typename<T>::name());
    }
};

void log_applied_options(const applied_operation& ops) {
    std::stringstream ss;
    ss << "[" << ops.block << "] ";
    ss << ops.trx_id.str() << " : "; /// golos::protocol::transaction_id_type
    operation_visitor ovisit;
    std::string op_name = ops.op.visit(ovisit);
    ss << "\"" << op_name << "\""; /// golos::protocol::operation
    ilog(ss.str());
}

struct operation_history_fixture : public add_operations_database_fixture {
    operations_map check_operations() {
        uint32_t head_block_num = db->head_block_num();
        ilog("Check history operations, block num is " + std::to_string(head_block_num));

        operations_map _found_ops;
        operation_visitor ovisit;
        for (uint32_t i = 1; i <= head_block_num; ++i) {
            msg_pack mo;
            mo.args = std::vector<fc::variant>({fc::variant(i), fc::variant(false)});
            auto ops = oh_plugin->get_ops_in_block(mo);
            for (const auto& o : ops) {
                auto itr = _found_ops.find(o.trx_id.str());
                if (itr == _found_ops.end()) {
                    _found_ops.insert(std::make_pair(o.trx_id.str(), o.op.visit(ovisit)));
                    log_applied_options(o);
                }
            }
        }
        return _found_ops;
    }
};

BOOST_FIXTURE_TEST_SUITE(operation_history_plugin, operation_history_fixture)

BOOST_AUTO_TEST_CASE(operation_history_blocks) {
    const uint32_t HISTORY_BLOCKS = 2;
    BOOST_TEST_MESSAGE("Testing: operation_history_blocks");
    initialize({
        {"history-blocks", std::to_string(HISTORY_BLOCKS)},
        {"history-whitelist-ops", OPERATIONS}
    });

    auto _added_ops = add_operations();
    auto _found_ops = check_operations();

    size_t _checked_ops_count = 0;
    for (auto it = _found_ops.begin(); it != _found_ops.end(); ++it) {
        auto itr = _added_ops.find(it->first);
        bool is_found = itr != _added_ops.end();
        BOOST_CHECK(is_found);
        if (is_found) {
            BOOST_CHECK_EQUAL(itr->second, it->second);
            if (itr->second == it->second) {
                ++_checked_ops_count;
            }
        } else {
            BOOST_TEST_MESSAGE("Operation \"" + it->second + "\" by \"" + it->first + "\" is not found");
        }
    }
    BOOST_CHECK_EQUAL(_checked_ops_count, HISTORY_BLOCKS);
}

BOOST_AUTO_TEST_CASE(black_options_postfix) {
    BOOST_TEST_MESSAGE("Testing: black_options_postfix");
    initialize({{"history-blacklist-ops", OPERATIONS}});

    auto _added_ops = add_operations();
    auto _found_ops = check_operations();

    size_t _checked_ops_count = 0;
    for (const auto& co : _added_ops) {
        auto itr = _found_ops.find(co.first);
        bool is_not_found = itr == _found_ops.end();
        BOOST_CHECK(is_not_found);
        if (is_not_found) {
            ++_checked_ops_count;
        } else {
            BOOST_TEST_MESSAGE("Operation \"" + co.second + "\" by \"" + co.first + "\" is found");
        }
    }
    BOOST_CHECK_EQUAL(_checked_ops_count, _added_ops.size());
}

BOOST_AUTO_TEST_CASE(white_options_postfix) {
    BOOST_TEST_MESSAGE("Testing: white_options_postfix");
    initialize({{"history-whitelist-ops", OPERATIONS}});

    auto _added_ops = add_operations();
    auto _found_ops = check_operations();

    size_t _checked_ops_count = 0;
    for (const auto& co : _added_ops) {
        auto itr = _found_ops.find(co.first);
        bool is_found = itr != _found_ops.end();
        BOOST_CHECK(is_found);
        if (is_found) {
            BOOST_CHECK_EQUAL(itr->second, co.second);
            if (itr->second == co.second) {
                ++_checked_ops_count;
            }
        } else {
            BOOST_TEST_MESSAGE("Operation \"" + co.second + "\" by \"" + co.first + "\" is not found");
        }
    }
    BOOST_CHECK_EQUAL(_checked_ops_count, _added_ops.size());
}

BOOST_AUTO_TEST_CASE(short_operation_history_blocks) {
    BOOST_TEST_MESSAGE("Testing: short_operation_history_blocks");
    initialize({{"history-whitelist-ops", "account_create_operation,delete_comment,comment"}});

    auto _added_ops = add_operations();
    auto _found_ops = check_operations();

    size_t _checked_ops_count = 0;
    for (auto it = _found_ops.begin(); it != _found_ops.end(); ++it) {
        auto itr = _added_ops.find(it->first);
        bool is_found = (itr != _added_ops.end());
        if (is_found) {
            BOOST_TEST_MESSAGE("Found operation \"" + it->second + "\" by \"" + it->first + "\"");
            BOOST_CHECK_EQUAL(itr->second, it->second);
            if (itr->second == it->second) {
                ++_checked_ops_count;
            }
        }
    }
    BOOST_CHECK_EQUAL(_checked_ops_count, 3);
}

BOOST_AUTO_TEST_SUITE_END()

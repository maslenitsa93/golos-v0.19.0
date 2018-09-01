#pragma once

#include <fc/api.hpp>
#include <fc/crypto/sha256.hpp>
#include <golos/protocol/types.hpp>
#include <string>

#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/utility.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>

namespace golos {
namespace plugins {
namespace auth_util {

using golos::plugins::json_rpc::msg_pack;


DEFINE_API_ARGS ( check_authority_signature, msg_pack, std::vector<protocol::public_key_type>)

class plugin final : public appbase::plugin<plugin> {
public:
    constexpr static const char *plugin_name = "auth_util";

    APPBASE_PLUGIN_REQUIRES((chain::plugin))

    static const std::string &name() {
        static std::string name = plugin_name;
        return name;
    }

    plugin();

    ~plugin();

    void set_program_options(boost::program_options::options_description &cli,
                             boost::program_options::options_description &cfg) override;

    void plugin_initialize(const boost::program_options::variables_map &options) override;

    void plugin_startup() override;

    void plugin_shutdown() override {
    }

    DECLARE_API (
        (check_authority_signature)
    )

private:
    struct plugin_impl;

    std::unique_ptr<plugin_impl> my;
};

}
}
} // golos::plugins::auth_util


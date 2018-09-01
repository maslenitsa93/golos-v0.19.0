#include <golos/protocol/config.hpp>
#include <golos/protocol/types.hpp>
#include <golos/protocol/exceptions.hpp>

#include <fc/crypto/base58.hpp>

namespace golos {

    template<>
    std::string get_logic_error_namespace<golos::logic_exception::error_types>() {
        return "";
    }

    namespace protocol {

        template<typename binary_key>
        binary_key key_data_from_string(const std::string &base58str) {
            std::string prefix(STEEMIT_ADDRESS_PREFIX);

            const size_t prefix_len = prefix.size();
            GOLOS_CHECK_VALUE(base58str.size() > prefix_len, "Public key is too short");
            GOLOS_CHECK_VALUE(base58str.substr(0, prefix_len) == prefix, "Invalid public key prefix", ("base58str", base58str));
            auto bin = fc::from_base58(base58str.substr(prefix_len));
            auto bin_key = fc::raw::unpack<binary_key>(bin);  // TODO catch exceptions from unpack
            auto key_data = bin_key.data;
            GOLOS_CHECK_VALUE(fc::ripemd160::hash(key_data.data, key_data.size())._hash[0] == bin_key.check,
                    "Invalid public key");
            return bin_key;
        }

        public_key_type::public_key_type() : key_data() {
        };

        public_key_type::public_key_type(const fc::ecc::public_key_data &data)
                : key_data(data) {
        };

        public_key_type::public_key_type(const fc::ecc::public_key &pubkey)
                : key_data(pubkey) {
        };

        public_key_type::public_key_type(const std::string &base58str) {
            // TODO:  Refactor syntactic checks into static is_valid()
            //        to make public_key_type API more similar to address API
            key_data = key_data_from_string<binary_key>(base58str).data;
        };


        public_key_type::operator fc::ecc::public_key_data() const {
            return key_data;
        };

        public_key_type::operator fc::ecc::public_key() const {
            return fc::ecc::public_key(key_data);
        };

        public_key_type::operator std::string() const {
            binary_key k;
            k.data = key_data;
            k.check = fc::ripemd160::hash(k.data.data, k.data.size())._hash[0];
            auto data = fc::raw::pack(k);
            return STEEMIT_ADDRESS_PREFIX +
                   fc::to_base58(data.data(), data.size());
        }

        bool operator==(const public_key_type &p1, const fc::ecc::public_key &p2) {
            return p1.key_data == p2.serialize();
        }

        bool operator==(const public_key_type &p1, const public_key_type &p2) {
            return p1.key_data == p2.key_data;
        }

        bool operator!=(const public_key_type &p1, const public_key_type &p2) {
            return p1.key_data != p2.key_data;
        }

        // extended_public_key_type

        extended_public_key_type::extended_public_key_type() : key_data() {
        };

        extended_public_key_type::extended_public_key_type(const fc::ecc::extended_key_data &data)
                : key_data(data) {
        };

        extended_public_key_type::extended_public_key_type(const fc::ecc::extended_public_key &extpubkey) {
            key_data = extpubkey.serialize_extended();
        };

        extended_public_key_type::extended_public_key_type(const std::string &base58str) {
            key_data = key_data_from_string<binary_key>(base58str).data;
        }

        extended_public_key_type::operator fc::ecc::extended_public_key() const {
            return fc::ecc::extended_public_key::deserialize(key_data);
        }

        extended_public_key_type::operator std::string() const {
            binary_key k;
            k.data = key_data;
            k.check = fc::ripemd160::hash(k.data.data, k.data.size())._hash[0];
            auto data = fc::raw::pack(k);
            return STEEMIT_ADDRESS_PREFIX +
                   fc::to_base58(data.data(), data.size());
        }

        bool operator==(const extended_public_key_type &p1, const fc::ecc::extended_public_key &p2) {
            return p1.key_data == p2.serialize_extended();
        }

        bool operator==(const extended_public_key_type &p1, const extended_public_key_type &p2) {
            return p1.key_data == p2.key_data;
        }

        bool operator!=(const extended_public_key_type &p1, const extended_public_key_type &p2) {
            return p1.key_data != p2.key_data;
        }

        // extended_private_key_type

        extended_private_key_type::extended_private_key_type() : key_data() {
        };

        extended_private_key_type::extended_private_key_type(const fc::ecc::extended_key_data &data)
                : key_data(data) {
        };

        extended_private_key_type::extended_private_key_type(const fc::ecc::extended_private_key &extprivkey) {
            key_data = extprivkey.serialize_extended();
        };

        extended_private_key_type::extended_private_key_type(const std::string &base58str) {
            key_data = key_data_from_string<binary_key>(base58str).data;
        }

        extended_private_key_type::operator fc::ecc::extended_private_key() const {
            return fc::ecc::extended_private_key::deserialize(key_data);
        }

        extended_private_key_type::operator std::string() const {
            binary_key k;
            k.data = key_data;
            k.check = fc::ripemd160::hash(k.data.data, k.data.size())._hash[0];
            auto data = fc::raw::pack(k);
            return STEEMIT_ADDRESS_PREFIX +
                   fc::to_base58(data.data(), data.size());
        }

        bool operator==(const extended_private_key_type &p1, const fc::ecc::extended_public_key &p2) {
            return p1.key_data == p2.serialize_extended();
        }

        bool operator==(const extended_private_key_type &p1, const extended_private_key_type &p2) {
            return p1.key_data == p2.key_data;
        }

        bool operator!=(const extended_private_key_type &p1, const extended_private_key_type &p2) {
            return p1.key_data != p2.key_data;
        }

    }
} // golos::protocol

namespace fc {
    using namespace std;

    void to_variant(const golos::protocol::public_key_type &var, fc::variant &vo) {
        vo = std::string(var);
    }

    void from_variant(const fc::variant &var, golos::protocol::public_key_type &vo) {
        vo = golos::protocol::public_key_type(var.as_string());
    }

    void to_variant(const golos::protocol::extended_public_key_type &var, fc::variant &vo) {
        vo = std::string(var);
    }

    void from_variant(const fc::variant &var, golos::protocol::extended_public_key_type &vo) {
        vo = golos::protocol::extended_public_key_type(var.as_string());
    }

    void to_variant(const golos::protocol::extended_private_key_type &var, fc::variant &vo) {
        vo = std::string(var);
    }

    void from_variant(const fc::variant &var, golos::protocol::extended_private_key_type &vo) {
        vo = golos::protocol::extended_private_key_type(var.as_string());
    }
} // fc

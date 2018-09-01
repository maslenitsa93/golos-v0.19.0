#include <golos/plugins/mongo_db/mongo_db_types.hpp>

namespace golos {
namespace plugins {
namespace mongo_db {

    void bmi_insert_or_replace(db_map& bmi, named_document doc) {
        auto& idx = bmi.get<hashed_idx>();
        auto it = idx.find(std::make_tuple(
            doc.collection_name,
            doc.key, doc.keyval, doc.is_removal));
        if (it != idx.end()) {
            idx.erase(it);
        }
        idx.emplace(std::move(doc));
    }
}
}
}
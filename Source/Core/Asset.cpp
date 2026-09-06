#include "Core/Asset.h"

namespace Elysium {

void AssetRegistry::Register(std::type_index payloadType, Entry entry) {
    Table()[payloadType] = std::move(entry);
}

const AssetRegistry::Entry* AssetRegistry::Find(std::type_index payloadType) {
    auto it = Table().find(payloadType);
    return it != Table().end() ? &it->second : nullptr;
}

std::unordered_map<std::type_index, AssetRegistry::Entry>& AssetRegistry::Table() {
    static std::unordered_map<std::type_index, Entry> table;
    return table;
}

}  // namespace Elysium

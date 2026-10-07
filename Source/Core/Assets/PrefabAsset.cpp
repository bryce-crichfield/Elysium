#include "Core/Assets/PrefabAsset.h"
#include "Services/LogService.h"

namespace Elysium {

bool PrefabAsset::Load() {
    if (!prefab_.Load(GetPath().GetFullPath())) return false;
    SetLoaded(true);
    LOG_DEBUGF("PrefabAsset", "Prefab loaded: %s", GetPath().c_str());
    return true;
}

REGISTER_ASSET_TYPE(Prefab, PrefabAsset);

}  // namespace Elysium

#include "Core/Assets/TileAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool TileAsset::Load() {
    try {
        tile_ = Tile::LoadFromXml(GetPath().GetFullPath());
        LOG_DEBUGF("TileAsset", "Loaded tile '%s' with %d variants",
                   tile_.name.c_str(), (int)tile_.variants.size());
        SetLoaded(true);
        return true;
    } catch (...) {
        LOG_ERRORF("TileAsset", "Failed to load tile: %s", GetPath().c_str());
        return false;
    }
}

REGISTER_ASSET_TYPE(Tile, TileAsset);

}  // namespace Elysium

#include "TileAsset.h"
#include "Core/Common.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"

namespace Elysium {

void TileAsset::LoadData() {
    try {
        tile_ = Tile::LoadFromXml(path_.GetFullPath());
        loaded_ = true;
    } catch (...) {
        LOG_ERRORF("TileAsset", "Failed to load: %s", path_.c_str());
    }
}

void TileAsset::OnLoaded(Services::AssetService& service) {
    if (tile_.sheet.path.empty()) return;
    Path sheetPath("Tiles/" + tile_.sheet.path);
    if (!service.IsAssetLoaded(sheetPath))
        service.LoadAsset(AssetType::TEXTURE, sheetPath);
}

}  // namespace Elysium

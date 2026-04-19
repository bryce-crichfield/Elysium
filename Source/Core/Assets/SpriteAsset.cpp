#include "SpriteAsset.h"
#include "Core/Common.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"

namespace Elysium {

void SpriteAsset::LoadData() {
    try {
        sprite_ = Sprite::LoadFromXml(path_.GetFullPath());
        loaded_ = true;
    } catch (...) {
        LOG_ERRORF("SpriteAsset", "Failed to load: %s", path_.c_str());
    }
}

void SpriteAsset::OnLoaded(Services::AssetService& service) {
    for (auto& [name, sheet] : sprite_.sheets) {
        Path sheetPath("Sprites/" + sheet.path);
        if (!service.IsAssetLoaded(sheetPath))
            service.LoadAsset(AssetType::TEXTURE, sheetPath);
    }
}

}  // namespace Elysium

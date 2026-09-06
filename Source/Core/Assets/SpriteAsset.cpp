#include "Core/Assets/SpriteAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool SpriteAsset::Load() {
    try {
        sprite_ = Sprite::LoadFromXml(GetPath().GetFullPath());
        LOG_DEBUGF("SpriteAsset", "Loaded sprite '%s' with %d sheets",
                   sprite_.name.c_str(), (int)sprite_.sheets.size());
        SetLoaded(true);
        return true;
    } catch (...) {
        LOG_ERRORF("SpriteAsset", "Failed to load sprite: %s", GetPath().c_str());
        return false;
    }
}

REGISTER_ASSET_TYPE(Sprite, SpriteAsset);

}  // namespace Elysium

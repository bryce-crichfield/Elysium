#include "Core/Assets/FontAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool FontAsset::Load() {
    ::Font font = ::LoadFont(GetPath().c_str());
    if (font.texture.id == 0) {
        LOG_ERRORF("FontAsset", "Failed to load font: %s", GetPath().c_str());
        return false;
    }

    nativeFont_ = font;
    font_ = Font{font.texture.id, font.baseSize, font.glyphCount};
    SetLoaded(true);
    LOG_INFO("FontAsset", "Font loaded successfully");
    return true;
}

void FontAsset::Unload() {
    if (IsLoaded()) {
        ::UnloadFont(nativeFont_);
        SetLoaded(false);
    }
}

REGISTER_ASSET_TYPE(Font, FontAsset);

}  // namespace Elysium

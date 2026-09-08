#include "Core/Assets/FontAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"
#include "raylib.h"

namespace Elysium {

struct FontAsset::Native {
    ::Font font{};
};

FontAsset::~FontAsset() = default;

bool FontAsset::Load() {
    ::Font font = ::LoadFont(GetPath().c_str());
    if (font.texture.id == 0) {
        LOG_ERRORF("FontAsset", "Failed to load font: %s", GetPath().c_str());
        return false;
    }

    native_ = std::make_unique<Native>();
    native_->font = font;
    font_ = Font{font.texture.id, font.baseSize, font.glyphCount};
    SetLoaded(true);
    LOG_INFO("FontAsset", "Font loaded successfully");
    return true;
}

void FontAsset::Unload() {
    if (IsLoaded() && native_) {
        ::UnloadFont(native_->font);
        SetLoaded(false);
    }
    native_.reset();
}

REGISTER_ASSET_TYPE(Font, FontAsset);

}  // namespace Elysium

#include "Core/Assets/TextureAsset.h"
#include "Core/Asset.h"
#include "Core/RaylibConvert.h"
#include "Services/LogService.h"

namespace Elysium {

struct TextureAsset::Native {
    ::Image image{};
    bool hasImage = false;
};

TextureAsset::~TextureAsset() = default;

bool TextureAsset::Load() {
    ::Image image = ::LoadImage(GetPath().c_str());
    if (image.data == nullptr) {
        LOG_ERRORF("TextureAsset", "Failed to load image data: %s", GetPath().c_str());
        return false;
    }

    LOG_DEBUGF("TextureAsset", "Image data loaded: %dx%d, format: %d, mipmaps: %d",
               image.width, image.height, image.format, image.mipmaps);
    native_ = std::make_unique<Native>();
    native_->image = image;
    native_->hasImage = true;
    return true;
}

bool TextureAsset::Finalize() {
    if (!native_ || !native_->hasImage) return false;

    ::Texture2D texture = ::LoadTextureFromImage(native_->image);
    ::UnloadImage(native_->image);
    native_->hasImage = false;

    if (texture.id == 0) {
        LOG_ERRORF("TextureAsset", "Failed to finalize texture: %s", GetPath().c_str());
        return false;
    }

    texture_ = FromRaylib(texture);
    SetLoaded(true);
    LOG_DEBUGF("TextureAsset", "Finalized texture: %s (ID: %d, %dx%d)",
               GetPath().c_str(), texture.id, texture.width, texture.height);
    return true;
}

void TextureAsset::Unload() {
    if (IsLoaded()) {
        ::UnloadTexture(ToRaylib(texture_));
        SetLoaded(false);
    }
    if (native_ && native_->hasImage) {
        ::UnloadImage(native_->image);
        native_->hasImage = false;
    }
    native_.reset();
}

REGISTER_ASSET_TYPE(Texture, TextureAsset);

}  // namespace Elysium

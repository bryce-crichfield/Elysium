#include "Core/Assets/TextureAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool TextureAsset::Load() {
    Image image = ::LoadImage(GetPath().c_str());
    if (image.data == nullptr) {
        LOG_ERRORF("TextureAsset", "Failed to load image data: %s", GetPath().c_str());
        return false;
    }

    LOG_DEBUGF("TextureAsset", "Image data loaded: %dx%d, format: %d, mipmaps: %d",
               image.width, image.height, image.format, image.mipmaps);
    imageData_ = image;
    hasImageData_ = true;
    return true;
}

bool TextureAsset::Finalize() {
    if (!hasImageData_) return false;

    Texture2D texture = ::LoadTextureFromImage(imageData_);
    ::UnloadImage(imageData_);
    hasImageData_ = false;

    if (texture.id == 0) {
        LOG_ERRORF("TextureAsset", "Failed to finalize texture: %s", GetPath().c_str());
        return false;
    }

    texture_ = Texture{texture.id, texture.width, texture.height, texture.mipmaps, texture.format};
    SetLoaded(true);
    LOG_DEBUGF("TextureAsset", "Finalized texture: %s (ID: %d, %dx%d)",
               GetPath().c_str(), texture.id, texture.width, texture.height);
    return true;
}

void TextureAsset::Unload() {
    if (IsLoaded()) {
        Texture2D texture{texture_.id, texture_.width, texture_.height, texture_.mipmaps, texture_.format};
        ::UnloadTexture(texture);
        SetLoaded(false);
    }
    if (hasImageData_) {
        ::UnloadImage(imageData_);
        hasImageData_ = false;
    }
}

REGISTER_ASSET_TYPE(Texture, TextureAsset);

}  // namespace Elysium

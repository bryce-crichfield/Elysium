#pragma once

#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class TextureAsset : public IAsset {
public:
    explicit TextureAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override {
        image_ = ::LoadImage(path_.c_str());
        if (image_.data) hasImage_ = true;
    }

    void Finalize() override {
        if (!hasImage_) return;
        texture_ = ::LoadTextureFromImage(image_);
        ::UnloadImage(image_);
        image_ = {};
        hasImage_ = false;
        if (texture_.id != 0) loaded_ = true;
    }

    bool NeedsFinalization() const override { return hasImage_; }

    void Unload() override {
        if (loaded_) { ::UnloadTexture(texture_); texture_ = {}; loaded_ = false; }
        if (hasImage_) { ::UnloadImage(image_); image_ = {}; hasImage_ = false; }
    }

    AssetType GetType() const override { return AssetType::TEXTURE; }
    Texture2D Get() const { return texture_; }

private:
    Image image_{};
    bool hasImage_ = false;
    Texture2D texture_{};
};

}  // namespace Elysium

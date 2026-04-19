#pragma once

#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class FontAsset : public IAsset {
public:
    explicit FontAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override {
        font_ = ::LoadFont(path_.c_str());
        if (font_.texture.id != 0) loaded_ = true;
    }

    void Unload() override {
        if (loaded_) { ::UnloadFont(font_); font_ = {}; loaded_ = false; }
    }

    AssetType GetType() const override { return AssetType::FONT; }
    Font Get() const { return font_; }

private:
    Font font_{};
};

}  // namespace Elysium

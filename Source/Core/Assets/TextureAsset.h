#pragma once

#include "Core/Graphics.h"
#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class TextureAsset : public AssetBase<TextureAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;      // background thread: reads raw image bytes
    bool Finalize() override;  // main thread: uploads to GPU
    bool NeedsFinalize() const override { return true; }
    void Unload() override;

    Texture& GetData() { return texture_; }

   private:
    Texture texture_{};
    Image imageData_{};
    bool hasImageData_ = false;
};

}  // namespace Elysium

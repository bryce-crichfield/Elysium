#pragma once

#include <memory>
#include "Core/Graphics.h"
#include "Core/Asset.h"

namespace Elysium {

class TextureAsset : public AssetBase<TextureAsset> {
   public:
    using AssetBase::AssetBase;
    ~TextureAsset() override;

    bool Load() override;      // background thread: reads raw image bytes
    bool Finalize() override;  // main thread: uploads to GPU
    bool NeedsFinalize() const override { return true; }
    void Unload() override;

    Texture& GetData() { return texture_; }

   private:
    struct Native;  // holds the raylib Image kept between Load() and Finalize()
    std::unique_ptr<Native> native_;
    Texture texture_{};
};

}  // namespace Elysium

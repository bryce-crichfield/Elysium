#pragma once

#include <memory>
#include "Core/Graphics.h"
#include "Core/Asset.h"

namespace Elysium {

class FontAsset : public AssetBase<FontAsset> {
   public:
    using AssetBase::AssetBase;
    ~FontAsset() override;

    bool Load() override;
    void Unload() override;

    Font& GetData() { return font_; }

   private:
    struct Native;  // holds the raylib Font handle for Unload()
    std::unique_ptr<Native> native_;
    Font font_{};
};

}  // namespace Elysium

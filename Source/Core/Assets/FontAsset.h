#pragma once

#include "Core/Graphics.h"
#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class FontAsset : public AssetBase<FontAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override;

    Font& GetData() { return font_; }

   private:
    Font font_{};
    ::Font nativeFont_{};
};

}  // namespace Elysium

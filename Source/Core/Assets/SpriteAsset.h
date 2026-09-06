#pragma once

#include "Core/Asset.h"
#include "Core/Sprite.h"

namespace Elysium {

class SpriteAsset : public AssetBase<SpriteAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override {}

    Sprite& GetData() { return sprite_; }

   private:
    Sprite sprite_;
};

}  // namespace Elysium

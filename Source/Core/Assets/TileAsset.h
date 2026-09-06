#pragma once

#include "Core/Asset.h"
#include "Core/Tile.h"

namespace Elysium {

class TileAsset : public AssetBase<TileAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override {}

    Tile& GetData() { return tile_; }

   private:
    Tile tile_;
};

}  // namespace Elysium

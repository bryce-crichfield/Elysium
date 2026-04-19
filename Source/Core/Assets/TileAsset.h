#pragma once

#include "Core/Asset.h"
#include "Core/Tile.h"

namespace Elysium {

class TileAsset : public IAsset {
public:
    explicit TileAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override;
    void OnLoaded(Services::AssetService& service) override;

    void Unload() override { tile_ = {}; loaded_ = false; }

    AssetType GetType() const override { return AssetType::TILE; }
    const Tile& Get() const { return tile_; }

private:
    Tile tile_;
};

}  // namespace Elysium

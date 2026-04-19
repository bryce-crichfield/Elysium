#pragma once

#include "Core/Asset.h"
#include "Core/Sprite.h"

namespace Elysium {

class SpriteAsset : public IAsset {
public:
    explicit SpriteAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override;
    void OnLoaded(Services::AssetService& service) override;

    void Unload() override { sprite_ = {}; loaded_ = false; }

    AssetType GetType() const override { return AssetType::SPRITE; }
    const Sprite& Get() const { return sprite_; }

private:
    Sprite sprite_;
};

}  // namespace Elysium

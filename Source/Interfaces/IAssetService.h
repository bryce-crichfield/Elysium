#pragma once

#include <unordered_map>
#include "Core/Asset.h"
#include "Core/Future.h"
#include "Core/Path.h"
#include "Core/Script.h"
#include "Core/Sprite.h"
#include "Core/Tile.h"
#include "raylib.h"

namespace Elysium::Services {

class IAssetService {
   public:
    virtual ~IAssetService() = default;

    virtual Future<Asset> LoadAsset(AssetType type, Path path) = 0;
    virtual Future<Asset> ReloadAsset(AssetType type, Path path) = 0;

    virtual void FinalizeAssets() = 0;
    virtual bool IsAssetLoaded(Path path) const = 0;

    virtual Asset* GetAsset(Path path) = 0;
    virtual Texture2D GetTexture(Path path) = 0;
    virtual Sound GetSound(Path path) = 0;
    virtual Music GetMusic(Path path) = 0;
    virtual Font GetFont(Path path) = 0;
    virtual Model GetModel(Path path) = 0;
    virtual Shader GetShader(Path path) = 0;
    virtual Sprite GetSprite(Path path) = 0;
    virtual Script GetScript(Path path) = 0;
    virtual Tile GetTile(Path path) = 0;

    virtual const std::unordered_map<Path, Asset>& GetAllAssets() const = 0;
};

}  // namespace Elysium::Services

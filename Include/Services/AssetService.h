#pragma once

#include "../Asset.h"
#include "MemoryTracker.h"
#include "raylib.h"
#include <string>
#include <unordered_map>

namespace Elysium::Services {

class AssetService {
public:
    void Initialize();
    void Shutdown();

    // Core asset loading - takes Asset object with name, path, type
    void LoadAsset(const Asset& asset);
    void FinalizeAssets(); // Convert raw data to GPU resources on main thread
    bool IsAssetLoaded(const AssetName& name) const;

    // Get assets by name
    Asset* GetAsset(const AssetName& name);
    Texture2D GetTexture(const AssetName& name);
    Sound GetSound(const AssetName& name);
    Music GetMusic(const AssetName& name);
    Font GetFont(const AssetName& name);
    Model GetModel(const AssetName& name);
    Shader GetShader(const AssetName& name);
    Sprite GetSprite(const AssetName& name);

    // Asset enumeration
    const std::unordered_map<AssetName, Asset>& GetAllAssets() const { return assetsByName_; }

private:
    void LoadAssetByType(Asset& asset);

    // Asset storage by name
    std::unordered_map<AssetName, Asset> assetsByName_;

    // Path deduplication - path -> name mapping
    std::unordered_map<std::string, AssetName> pathToName_;
};

} // namespace Elysium::Services

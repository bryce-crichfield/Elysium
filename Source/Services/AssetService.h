#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "Asset.h"
#include "Core/Future.h"
#include "Service.h"
#include "raylib.h"

namespace Elysium {
class TaskService;
}

namespace Elysium::Services {

class AssetService : public Elysium::Service {
   public:
    AssetService();

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Async asset loading — I/O runs on background thread,
    // cache insertion happens on main thread via Future continuations
    Future<Asset> LoadAsset(AssetType type, Path path);
    Future<Asset> ReloadAsset(AssetType type, Path path);

    void FinalizeAssets();  // Convert raw data to GPU resources on main thread
    bool IsAssetLoaded(Path path) const;

    // Get assets by name
    Asset* GetAsset(Path path);
    Texture2D GetTexture(Path path);
    Sound GetSound(Path path);
    Music GetMusic(Path path);
    Font GetFont(Path path);
    Model GetModel(Path path);
    Shader GetShader(Path path);
    Sprite GetSprite(Path path);
    Script GetScript(Path path);
    Tile   GetTile(Path path);

    // Asset enumeration
    const std::unordered_map<Path, Asset>& GetAllAssets() const { return assetsByPath_; }

   private:
    // Performs I/O to load raw asset data — thread-safe, does NOT touch assetsByPath_
    static Asset LoadAssetData(AssetType type, Path path);

    // Asset storage by path (only written from main thread)
    std::unordered_map<Path, Asset> assetsByPath_;

    // Track pending futures so we know when to finalize
    std::vector<Future<Asset>> pendingFutures_;
    bool needsFinalization_ = false;
};

}  // namespace Elysium::Services

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "Asset.h"
#include "Core/Future.h"
#include "Interfaces/IAssetService.h"
#include "Service.h"
#include "raylib.h"

namespace Elysium {
class TaskService;
}

namespace Elysium::Services {

class AssetService : public Elysium::Service, public IAssetService {
   public:
    AssetService(ServiceLocator& registry);

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Async asset loading — I/O runs on background thread,
    // cache insertion happens on main thread via Future continuations
    Future<Asset> LoadAsset(AssetType type, Path path) override;
    Future<Asset> ReloadAsset(AssetType type, Path path) override;

    void FinalizeAssets() override;  // Convert raw data to GPU resources on main thread
    bool IsAssetLoaded(Path path) const override;

    // Get assets by name
    Asset* GetAsset(Path path) override;
    Texture2D GetTexture(Path path) override;
    Sound GetSound(Path path) override;
    Music GetMusic(Path path) override;
    Font GetFont(Path path) override;
    Model GetModel(Path path) override;
    Shader GetShader(Path path) override;
    Sprite GetSprite(Path path) override;
    Script GetScript(Path path) override;
    Tile   GetTile(Path path) override;

    // Asset enumeration
    const std::unordered_map<Path, Asset>& GetAllAssets() const override { return assetsByPath_; }

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

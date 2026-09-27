#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Core/Future.h"
#include "Core/Asset.h"
#include "Core/Path.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/IAssetService.h"

namespace Elysium {
class TaskService;
}

namespace Elysium::Services {

class AssetService : public IAssetService {
   public:
    AssetService(ServiceLocator& registry);

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    Future<IAsset*> ReloadAsset(IAsset* asset) override;

    void FinalizeAssets() override;  // Convert raw data to GPU/audio resources on main thread
    bool IsAssetLoaded(Path path) const override;

    IAsset* GetAsset(Path path) override;

    // Asset enumeration
    const std::unordered_map<Path, std::unique_ptr<IAsset>>& GetAllAssets() const override { return assetsByPath_; }

   private:
    ServiceLocator& registry_;  // reaches TaskService for async loads

    // Performs I/O to load raw asset data — thread-safe, does NOT touch assetsByPath_.
    // Returns an owning pointer on success, or nullptr if IAsset::Load() failed.
    static IAsset* LoadAssetData(const std::function<std::unique_ptr<IAsset>(Path)>& factory, Path path);

    // Main-thread continuation for a completed background load. Takes ownership of `raw`.
    void FinishLoad(Path path, IAsset* raw);

    // Resolves every caller-facing future still waiting on `path` with `result`.
    void NotifyWaiters(const Path& path, IAsset* result);

    // Asset storage by path (only written from main thread). Holds fully-loaded assets
    // plus assets whose data is loaded but still awaiting main-thread finalization.
    std::unordered_map<Path, std::unique_ptr<IAsset>> assetsByPath_;

    // Paths with a background load in flight — used to dedupe concurrent requests
    // without parking a half-constructed placeholder in assetsByPath_.
    std::unordered_set<Path> inFlightPaths_;

    // Caller-facing futures handed out by LoadAssetRaw. Polled every Update() so their
    // Then() continuations fire on the main thread; dropped once resolved.
    std::vector<std::pair<Path, Future<IAsset*>>> waiters_;

    // Background loads whose FinishLoad continuation has not run yet. Finalization is
    // deferred until this hits zero so a batch of loads finalizes together.
    std::uint32_t outstandingLoads_ = 0;
    bool needsFinalization_ = false;
protected:
    // Async asset loading — I/O runs on background thread,
    // cache insertion happens on main thread via Future continuations
    Future<IAsset*> LoadAssetRaw(Path path, std::function<std::unique_ptr<IAsset>(Path)> factory) override;
    // Synchronous load on the main thread; finalizes immediately if the asset needs it.
    IAsset* LoadAssetNowRaw(Path path, std::function<std::unique_ptr<IAsset>(Path)> factory, bool reload) override;
};

}  // namespace Elysium::Services

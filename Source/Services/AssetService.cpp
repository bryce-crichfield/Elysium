#include "Services/AssetService.h"
#include <algorithm>
#include "Core/Assets/SpriteAsset.h"
#include "Core/Assets/TextureAsset.h"
#include "Core/Assets/TileAsset.h"
#include "Core/Common.h"
#include "Interfaces/ITaskService.h"
#include "Services/LogService.h"
#include "Services/TaskService.h"

namespace Elysium::Services {

AssetService::AssetService(ServiceLocator& registry) : registry_(registry) {}

void AssetService::Initialize() {
    assetsByPath_.clear();
    LOG_INFO("AssetService", "Initialized");
}

void AssetService::Shutdown() {
    for (auto& [path, asset] : assetsByPath_) {
        if (asset->IsLoaded()) {
            asset->Unload();
            LOG_INFOF("AssetService", "Unloaded asset: %s", asset->GetPath().c_str());
        }
    }

    assetsByPath_.clear();
    inFlightPaths_.clear();
    waiters_.clear();
    outstandingLoads_ = 0;
    needsFinalization_ = false;
    LOG_INFO("AssetService", "Shutdown complete");
}

void AssetService::Update(float deltaTime) {
    Profile;

    // Finalize a batch once every background load's continuation has landed.
    if (needsFinalization_ && outstandingLoads_ == 0) {
        FinalizeAssets();
        needsFinalization_ = false;
    }

    // Fire caller-facing Then() continuations for loads that have resolved, and drop
    // futures that fire-and-forget callers have already discarded.
    std::erase_if(waiters_, [](std::pair<Path, Future<IAsset*>>& w) {
        return w.second.Poll() || w.second.Abandoned();
    });
}

Future<IAsset*> AssetService::LoadAssetRaw(Path path, std::function<std::unique_ptr<IAsset>(Path)> factory) {
    Future<IAsset*> caller;
    // Track every handed-out future so Update() can pump its continuations.
    waiters_.push_back({path, caller});

    auto existing = assetsByPath_.find(path);
    if (existing != assetsByPath_.end()) {
        if (existing->second->IsLoaded()) {
            LOG_DEBUGF("AssetService", "Asset already loaded: %s", path.c_str());
            caller.Resolve(existing->second.get());
        }
        // else: data loaded, awaiting finalization — FinalizeAssets() resolves the waiter.
        return caller;
    }

    if (inFlightPaths_.count(path)) {
        // A background load is already running for this path; piggyback on it.
        return caller;
    }

    inFlightPaths_.insert(path);
    outstandingLoads_++;

    auto& taskService = registry_.Get<ITaskService>();

    Future<IAsset*> loadFuture = taskService.Submit<IAsset*>(
        std::function<IAsset*()>([factory, path]() -> IAsset* {
            return LoadAssetData(factory, path);
        }));

    // Runs on the main thread via TaskService::Update().
    loadFuture.Then([this, path](IAsset* raw) mutable {
        FinishLoad(path, raw);
    });

    return caller;
}

IAsset* AssetService::LoadAssetNowRaw(Path path, std::function<std::unique_ptr<IAsset>(Path)> factory, bool reload) {
    auto existing = assetsByPath_.find(path);
    if (existing != assetsByPath_.end()) {
        if (!reload && existing->second->IsLoaded()) return existing->second.get();
        existing->second->Unload();
        assetsByPath_.erase(existing);
    }

    std::unique_ptr<IAsset> asset(LoadAssetData(factory, path));
    if (!asset) return nullptr;
    if (asset->NeedsFinalize() && !asset->IsLoaded() && !asset->Finalize()) {
        LOG_WARNINGF("AssetService", "Finalize failed for asset: %s", path.c_str());
        return nullptr;
    }

    IAsset* stored = asset.get();
    assetsByPath_[path] = std::move(asset);
    LOG_DEBUGF("AssetService", "Loaded asset now: %s", path.c_str());
    NotifyWaiters(path, stored);  // anyone waiting on an async load of the same path
    return stored;
}

Future<IAsset*> AssetService::ReloadAsset(IAsset* asset) {
    if (!asset) return Future<IAsset*>{};

    Path path = asset->GetPath();
    auto factory = asset->GetFactory();

    auto it = assetsByPath_.find(path);
    if (it != assetsByPath_.end()) {
        it->second->Unload();  // safe on partially-loaded assets; frees any staged data
        assetsByPath_.erase(it);
    }
    // If a load is still in flight for this path, LoadAssetRaw will piggyback on it
    // rather than spawning a competing task — no stale continuation can clobber the map.

    LOG_INFOF("AssetService", "Reloading asset: %s", path.c_str());
    return LoadAssetRaw(path, factory);
}

bool AssetService::IsAssetLoaded(Path path) const {
    auto it = assetsByPath_.find(path);
    return it != assetsByPath_.end() && it->second->IsLoaded();
}

IAsset* AssetService::GetAsset(Path path) {
    auto it = assetsByPath_.find(path);
    if (it != assetsByPath_.end() && it->second->IsLoaded()) {
        return it->second.get();
    }
    return nullptr;
}

// Thread-safe I/O — does NOT touch assetsByPath_. Returns nullptr when Load() fails so
// the continuation can drop the entry and leave the path retryable.
IAsset* AssetService::LoadAssetData(const std::function<std::unique_ptr<IAsset>(Path)>& factory, Path path) {
    auto asset = factory(path);
    LOG_DEBUGF("AssetService", "Loading asset from path %s", path.c_str());
    if (!asset->Load()) {
        LOG_WARNINGF("AssetService", "Load() failed for asset: %s", path.c_str());
        return nullptr;
    }

    return asset.release();
}

void AssetService::FinishLoad(Path path, IAsset* raw) {
    inFlightPaths_.erase(path);
    outstandingLoads_--;

    if (!raw) {
        LOG_WARNINGF("AssetService", "Failed to load asset: %s", path.c_str());
        NotifyWaiters(path, nullptr);  // unblock callers; path stays retryable
        return;
    }

    std::unique_ptr<IAsset> owned(raw);

    // LoadAssetNow got there first while this was in flight: keep that copy, since
    // callers may already hold pointers into it.
    if (auto it = assetsByPath_.find(path); it != assetsByPath_.end() && it->second->IsLoaded()) {
        owned->Unload();
        NotifyWaiters(path, it->second.get());
        return;
    }

    // Sprites/tiles reference a sheet texture by path — kick off that load too.
    if (auto* spriteAsset = dynamic_cast<SpriteAsset*>(owned.get())) {
        for (auto& [sheetName, sheet] : spriteAsset->GetData().sheets) {
            Path sheetPath(sheet.path);
            if (!IsAssetLoaded(sheetPath)) {
                LOG_DEBUGF("AssetService", "Loading sheet texture: %s", sheetPath.c_str());
                LoadAsset<Texture>(sheetPath);
            }
            for (const std::string* map : {&sheet.normalPath, &sheet.emissionPath}) {
                if (!map->empty() && !IsAssetLoaded(Path(*map))) LoadAsset<Texture>(Path(*map));
            }
        }
    } else if (auto* tileAsset = dynamic_cast<TileAsset*>(owned.get())) {
        const Tile& tile = tileAsset->GetData();
        if (!tile.sheet.path.empty()) {
            Path sheetPath("Tiles/" + tile.sheet.path);
            if (!IsAssetLoaded(sheetPath)) {
                LOG_DEBUGF("AssetService", "Loading tile sheet texture: %s", sheetPath.c_str());
                LoadAsset<Texture>(sheetPath);
            }
        }
    }

    IAsset* stored = owned.get();
    assetsByPath_[path] = std::move(owned);

    if (stored->IsLoaded()) {
        LOG_INFOF("AssetService", "Loaded asset: %s", path.c_str());
        NotifyWaiters(path, stored);
    } else {
        LOG_INFOF("AssetService", "Loaded asset data: %s (awaiting finalization)", path.c_str());
        needsFinalization_ = true;  // waiter is resolved by FinalizeAssets()
    }
}

void AssetService::NotifyWaiters(const Path& path, IAsset* result) {
    for (auto& [waiterPath, future] : waiters_) {
        if (waiterPath == path && !future.IsReady()) {
            future.Resolve(result);
        }
    }
}

void AssetService::FinalizeAssets() {
    LOG_INFO("AssetService", "Finalizing assets on main thread");

    std::vector<Path> failed;
    for (auto& [path, asset] : assetsByPath_) {
        if (!asset->NeedsFinalize() || asset->IsLoaded()) continue;

        if (asset->Finalize()) {
            NotifyWaiters(path, asset.get());
        } else {
            LOG_WARNINGF("AssetService", "Finalize failed, dropping asset: %s", path.c_str());
            NotifyWaiters(path, nullptr);
            failed.push_back(path);
        }
    }

    for (const auto& path : failed) {
        assetsByPath_.erase(path);  // leave the path retryable
    }

    LOG_INFO("AssetService", "Asset finalization complete");
}

}  // namespace Elysium::Services

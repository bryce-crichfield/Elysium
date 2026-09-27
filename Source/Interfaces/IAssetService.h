#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include "Core/Asset.h"
#include "Core/Future.h"
#include "Core/Asset.h"
#include "Core/Path.h"
#include "Interfaces/IService.h"

namespace Elysium::Services {

// Owns every loaded IAsset. Callers get a non-owning pointer (IAsset*, or a payload
// pointer via Get<T>/GetData<T>) valid until the next Reload/Unload/Shutdown of that path.
class IAssetService : public IService {
   public:

    template <typename Payload>
    Future<IAsset*> LoadAsset(Path path) {
        const auto* entry = AssetRegistry::Find(std::type_index(typeid(Payload)));
        assert(entry && "No REGISTER_ASSET_TYPE(Payload, Concrete) registered for this payload type");
        if (!entry) return {};
        return LoadAssetRaw(std::move(path), entry->factory);
    }

    // Loads on the calling (main) thread and returns the payload right away, reusing an
    // already-loaded copy. For data needed immediately, like prefabs during a scene load.
    // Returns nullptr if the load fails.
    template <typename Payload>
    Payload* LoadAssetNow(Path path) {
        return LoadNow<Payload>(std::move(path), false);
    }

    // Like LoadAssetNow, but always rereads the file (e.g. after the editor rewrote it).
    // Pointers to the previous payload dangle afterwards.
    template <typename Payload>
    Payload* ReloadAssetNow(Path path) {
        return LoadNow<Payload>(std::move(path), true);
    }

    virtual Future<IAsset*> ReloadAsset(IAsset* asset) = 0;

    virtual void FinalizeAssets() = 0;
    virtual bool IsAssetLoaded(Path path) const = 0;

    virtual IAsset* GetAsset(Path path) = 0;

    // Returns nullptr if `asset` is null or isn't actually backing a Payload.
    template <typename Payload>
    Payload* GetData(IAsset* asset) {
        if (!asset) return nullptr;
        const auto* entry = AssetRegistry::Find(std::type_index(typeid(Payload)));
        assert(entry && "No REGISTER_ASSET_TYPE(Payload, Concrete) registered for this payload type");
        if (!entry) return nullptr;
        return static_cast<Payload*>(entry->getData(asset));
    }

    // Typed lookup, e.g. Get<Texture>(path). Returns nullptr if the asset isn't loaded.
    template <typename Payload>
    Payload* Get(Path path) {
        return GetData<Payload>(GetAsset(std::move(path)));
    }

    virtual const std::unordered_map<Path, std::unique_ptr<IAsset>>& GetAllAssets() const = 0;
protected:
    // Template can't be virtual, so LoadAsset<T> forwards here with T's registered factory.
    virtual Future<IAsset*> LoadAssetRaw(Path path, std::function<std::unique_ptr<IAsset>(Path)> factory) = 0;
    virtual IAsset* LoadAssetNowRaw(Path path, std::function<std::unique_ptr<IAsset>(Path)> factory, bool reload) = 0;

   private:
    template <typename Payload>
    Payload* LoadNow(Path path, bool reload) {
        const auto* entry = AssetRegistry::Find(std::type_index(typeid(Payload)));
        assert(entry && "No REGISTER_ASSET_TYPE(Payload, Concrete) registered for this payload type");
        if (!entry) return nullptr;
        return GetData<Payload>(LoadAssetNowRaw(std::move(path), entry->factory, reload));
    }
};

}  // namespace Elysium::Services

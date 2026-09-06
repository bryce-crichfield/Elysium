#pragma once

#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include "Core/Asset.h"
#include "Core/Path.h"


namespace Elysium {

// Base type for everything IAssetService loads. Owned exclusively by AssetService;
// callers only ever see a raw observing pointer (see IAssetService::Get<T>).
class IAsset {
   public:
    explicit IAsset(Path path) : path_(std::move(path)) {}
    virtual ~IAsset() = default;

    IAsset(const IAsset&) = delete;
    IAsset& operator=(const IAsset&) = delete;

    const Path& GetPath() const { return path_; }
    bool IsLoaded() const { return loaded_; }

    // Runs on a background thread, on an instance not yet visible to anything else.
    virtual bool Load() = 0;

    // Runs on the main thread after a successful Load(), for assets needing a
    // GPU/audio-device upload step (Texture, Sound). Default: nothing to do.
    virtual bool Finalize() { return true; }
    virtual bool NeedsFinalize() const { return false; }

    virtual void Unload() = 0;

    // Builds a fresh instance of this asset's concrete type. Provided by AssetBase<T>.
    virtual std::function<std::unique_ptr<IAsset>(Path)> GetFactory() const = 0;

   protected:
    void SetLoaded(bool loaded) { loaded_ = loaded; }

   private:
    Path path_;
    bool loaded_ = false;
};

// F-Bound helper. Derive as `class MyAsset : public AssetBase<MyAsset>` to get
// GetFactory() (and constructor forwarding) for free.
template <typename Derived>
class AssetBase : public IAsset {
   public:
    using IAsset::IAsset;

    std::function<std::unique_ptr<IAsset>(Path)> GetFactory() const override {
        return [](Path p) -> std::unique_ptr<IAsset> {
            return std::make_unique<Derived>(std::move(p));
        };
    }
};

// Maps a payload type (Texture, Script, Sprite, ...) to the IAsset subclass that
// loads/stores it, so LoadAsset<Payload>/Get<Payload> never needs to name it.
class AssetRegistry {
   public:
    using Factory = std::function<std::unique_ptr<IAsset>(Path)>;
    using DataAccessor = std::function<void*(IAsset*)>;

    struct Entry {
        Factory factory;
        DataAccessor getData;
    };

    static void Register(std::type_index payloadType, Entry entry);
    static const Entry* Find(std::type_index payloadType);

   private:
    static std::unordered_map<std::type_index, Entry>& Table();
};

template <typename AssetPayload, typename AssetType>
struct AssetTypeRegistrar {
    AssetTypeRegistrar() {
        AssetRegistry::Register(
            std::type_index(typeid(AssetPayload)),
            AssetRegistry::Entry{
                [](Path p) -> std::unique_ptr<IAsset> { return std::make_unique<AssetType>(std::move(p)); },
                [](IAsset* asset) -> void* {
                    auto* assetType = dynamic_cast<AssetType*>(asset);
                    return assetType ? static_cast<void*>(&assetType->GetData()) : nullptr;
                }});
    }
};

}  // namespace Elysium

#define REGISTER_ASSET_TYPE(AssetPayload, AssetType) \
    static ::Elysium::AssetTypeRegistrar<AssetPayload, AssetType> _assetTypeRegistrar_##AssetType {}

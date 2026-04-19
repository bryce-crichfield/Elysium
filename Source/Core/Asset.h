#pragma once

#include "Core/Event.h"
#include "Core/Path.h"

#ifndef ASSETS_PATH
#define ASSETS_PATH "./Assets/"
#endif

namespace Elysium {

enum class AssetType {
    TEXTURE,
    SOUND,
    MUSIC,
    FONT,
    MODEL,
    SHADER,
    SPRITE,
    SCRIPT,
    TILE,
    CHARACTER
};

namespace Services { class AssetService; }

class IAsset {
public:
    explicit IAsset(Path path) : path_(std::move(path)) {}
    virtual ~IAsset() = default;

    // Off-thread: pure I/O — no GPU or OpenAL calls
    virtual void LoadData() = 0;

    // Main-thread: promote raw data to GPU resources (e.g. Image → Texture2D)
    virtual void Finalize() {}
    virtual bool NeedsFinalization() const { return false; }

    // Main-thread: trigger dependent asset loads (e.g. sprite sheet textures)
    virtual void OnLoaded(Services::AssetService&) {}

    // Main-thread: release GPU/audio resources
    virtual void Unload() = 0;

    virtual AssetType GetType() const = 0;

    bool IsLoaded() const { return loaded_; }
    Path GetPath() const { return path_; }

protected:
    Path path_;
    bool loaded_ = false;
};

// --- Asset events (unchanged API) ---

enum class AssetEventType { LOADED, UNLOADED, RELOADED };

class AssetEvent : public Event {
public:
    AssetEvent(Path path, AssetEventType type) : path_(path), eventType_(type) {}
    Path GetPath() const { return path_; }
    AssetEventType GetType() const { return eventType_; }
private:
    Path path_;
    AssetEventType eventType_;
};

class IAssetEventListener : public IEventListener {
public:
    virtual ~IAssetEventListener() = default;
    virtual void OnAssetEvent(const AssetEvent& event) = 0;
};

}  // namespace Elysium

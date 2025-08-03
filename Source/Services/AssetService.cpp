#include "Services/AssetService.h"
#include <unordered_map>

namespace Elysium::Services {

static std::unordered_map<std::string, Asset> assetCache;

void AssetService::Initialize() {
    assetCache.clear();
}

void AssetService::Shutdown() {
    for (auto& pair : assetCache) {
        pair.second.Unload();
    }
    assetCache.clear();
}

bool AssetService::GetAsset(const std::string& path, Asset* out) {
    auto it = assetCache.find(path);
    if (it != assetCache.end() && it->second.IsLoaded()) {
        if (out) {
            *out = it->second;
        }
        return true;
    }
    return false;
}

Texture2D AssetService::LoadTexture(const std::string& path) {
    auto it = assetCache.find(path);
    if (it != assetCache.end() && it->second.IsLoaded() && it->second.GetType() == AssetType::TEXTURE) {
        return it->second.GetTexture();
    }
    
    Texture2D texture = ::LoadTexture(path.c_str());
    Asset asset(AssetType::TEXTURE, path);
    asset.SetTexture(texture);
    assetCache[path] = asset;
    return texture;
}

Sound AssetService::LoadSound(const std::string& path) {
    auto it = assetCache.find(path);
    if (it != assetCache.end() && it->second.IsLoaded() && it->second.GetType() == AssetType::SOUND) {
        return it->second.GetSound();
    }
    
    Sound sound = ::LoadSound(path.c_str());
    Asset asset(AssetType::SOUND, path);
    asset.SetSound(sound);
    assetCache[path] = asset;
    return sound;
}

void AssetService::UnloadTexture(const std::string& path) {
    auto it = assetCache.find(path);
    if (it != assetCache.end() && it->second.GetType() == AssetType::TEXTURE) {
        it->second.Unload();
        assetCache.erase(it);
    }
}

void AssetService::UnloadSound(const std::string& path) {
    auto it = assetCache.find(path);
    if (it != assetCache.end() && it->second.GetType() == AssetType::SOUND) {
        it->second.Unload();
        assetCache.erase(it);
    }
}

} // namespace Elysium::Services
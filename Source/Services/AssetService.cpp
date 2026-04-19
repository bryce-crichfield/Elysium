#include "Core/Common.h"
#include "Services/AssetService.h"
#include "Core/Application.h"
#include "Core/Assets/AssetFactory.h"
#include "Services/LogService.h"
#include "Services/TaskService.h"
#include "imgui.h"
#include "raylib.h"

namespace Elysium::Services {

AssetService::AssetService() {
    name_ = "AssetService";
}

void AssetService::Initialize() {
    assets_.clear();
    LOG_INFO("AssetService", "Initialized");
}

void AssetService::Shutdown() {
    for (auto& [path, asset] : assets_) {
        if (asset->IsLoaded()) {
            asset->Unload();
            LOG_INFOF("AssetService", "Unloaded asset: %s", asset->GetPath().c_str());
        }
    }
    assets_.clear();
    pendingFutures_.clear();
    LOG_INFO("AssetService", "Shutdown complete");
}

void AssetService::Update(float deltaTime) {
    Profile;

    if (!needsFinalization_) return;

    bool allDone = true;
    for (auto& f : pendingFutures_) {
        if (!f.IsReady()) { allDone = false; break; }
    }
    if (allDone) {
        FinalizeAssets();
        pendingFutures_.clear();
        needsFinalization_ = false;
    }
}

Future<std::shared_ptr<IAsset>> AssetService::LoadAsset(AssetType type, Path path) {
    auto existing = assets_.find(path);
    if (existing != assets_.end()) {
        Future<std::shared_ptr<IAsset>> future;
        if (existing->second->IsLoaded()) {
            LOG_DEBUGF("AssetService", "Asset already loaded: %s", path.c_str());
            future.Resolve(existing->second);
        }
        return future;
    }

    auto asset = CreateAsset(type, path);
    assets_[path] = asset;

    auto& taskService = Application::GetInstance().GetService<TaskService>();

    Future<std::shared_ptr<IAsset>> future = taskService.Submit<std::shared_ptr<IAsset>>(
        std::function<std::shared_ptr<IAsset>()>([asset]() -> std::shared_ptr<IAsset> {
            asset->LoadData();
            return asset;
        }));

    future.Then([this](const std::shared_ptr<IAsset>& loaded) {
        if (loaded->IsLoaded() || loaded->NeedsFinalization()) {
            if (loaded->IsLoaded()) {
                LOG_INFOF("AssetService", "Loaded asset: %s", loaded->GetPath().c_str());
            } else {
                LOG_INFOF("AssetService", "Raw data ready: %s (needs finalization)", loaded->GetPath().c_str());
                needsFinalization_ = true;
            }
            loaded->OnLoaded(*this);
        } else {
            LOG_WARNINGF("AssetService", "Failed to load asset: %s", loaded->GetPath().c_str());
        }
    });

    pendingFutures_.push_back(future);
    needsFinalization_ = true;

    return future;
}

Future<std::shared_ptr<IAsset>> AssetService::ReloadAsset(AssetType type, Path path) {
    auto it = assets_.find(path);
    if (it != assets_.end()) {
        if (it->second->IsLoaded()) it->second->Unload();
        assets_.erase(it);
    }
    LOG_INFOF("AssetService", "Reloading asset: %s", path.c_str());
    return LoadAsset(type, path);
}

bool AssetService::IsAssetLoaded(Path path) const {
    auto it = assets_.find(path);
    return it != assets_.end() && it->second->IsLoaded();
}

IAsset* AssetService::GetAsset(Path path) {
    auto it = assets_.find(path);
    if (it != assets_.end() && it->second->IsLoaded()) return it->second.get();
    return nullptr;
}

void AssetService::FinalizeAssets() {
    LOG_INFO("AssetService", "Finalizing assets on main thread");
    for (auto& [path, asset] : assets_) {
        if (asset->NeedsFinalization() && !asset->IsLoaded()) {
            asset->Finalize();
            if (asset->IsLoaded()) {
                LOG_DEBUGF("AssetService", "Finalized: %s", path.c_str());
            } else {
                LOG_ERRORF("AssetService", "Finalization failed: %s", path.c_str());
            }
        }
    }
    LOG_INFO("AssetService", "Asset finalization complete");
}

// --- Typed accessors ---

Texture2D AssetService::GetTexture(Path path) {
    if (auto* a = dynamic_cast<TextureAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Sound AssetService::GetSound(Path path) {
    if (auto* a = dynamic_cast<SoundAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Music AssetService::GetMusic(Path path) {
    if (auto* a = dynamic_cast<MusicAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Font AssetService::GetFont(Path path) {
    if (auto* a = dynamic_cast<FontAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Model AssetService::GetModel(Path path) {
    if (auto* a = dynamic_cast<ModelAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Shader AssetService::GetShader(Path path) {
    if (auto* a = dynamic_cast<ShaderAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Sprite AssetService::GetSprite(Path path) {
    if (auto* a = dynamic_cast<SpriteAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Script AssetService::GetScript(Path path) {
    if (auto* a = dynamic_cast<ScriptAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Tile AssetService::GetTile(Path path) {
    if (auto* a = dynamic_cast<TileAsset*>(GetAsset(path))) return a->Get();
    return {};
}

Character AssetService::GetCharacter(Path path) {
    if (auto* a = dynamic_cast<CharacterAsset*>(GetAsset(path))) return a->Get();
    return {};
}

void AssetService::SetCharacter(Path path, const Character& character) {
    auto it = assets_.find(path);
    if (it != assets_.end()) {
        if (auto* a = dynamic_cast<CharacterAsset*>(it->second.get())) {
            a->Set(character);
            return;
        }
    }
    auto asset = std::make_shared<CharacterAsset>(path);
    asset->Set(character);
    assets_[path] = asset;
}

}  // namespace Elysium::Services

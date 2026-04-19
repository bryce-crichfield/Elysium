#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Asset.h"
#include "Core/Character.h"
#include "Core/Future.h"
#include "Service.h"
#include "raylib.h"

#include "Core/Script.h"
#include "Core/Sprite.h"
#include "Core/Tile.h"

namespace Elysium {
class TaskService;
}

namespace Elysium::Services {

class AssetService : public Elysium::Service {
public:
    AssetService();

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    Future<std::shared_ptr<IAsset>> LoadAsset(AssetType type, Path path);
    Future<std::shared_ptr<IAsset>> ReloadAsset(AssetType type, Path path);

    void FinalizeAssets();
    bool IsAssetLoaded(Path path) const;

    IAsset* GetAsset(Path path);

    // Typed value accessors — return empty on miss or wrong type
    Texture2D GetTexture(Path path);
    Sound     GetSound(Path path);
    Music     GetMusic(Path path);
    Font      GetFont(Path path);
    Model     GetModel(Path path);
    Shader    GetShader(Path path);
    Sprite    GetSprite(Path path);
    Script    GetScript(Path path);
    Tile      GetTile(Path path);
    Character GetCharacter(Path path);

    // Direct typed pointer — null on miss or wrong type
    template<typename T>
    T* GetTyped(Path path) {
        IAsset* base = GetAsset(path);
        return base ? dynamic_cast<T*>(base) : nullptr;
    }

    void SetCharacter(Path path, const Character& character);

    const std::unordered_map<Path, std::shared_ptr<IAsset>>& GetAllAssets() const { return assets_; }

private:
    std::unordered_map<Path, std::shared_ptr<IAsset>> assets_;
    std::vector<Future<std::shared_ptr<IAsset>>> pendingFutures_;
    bool needsFinalization_ = false;
};

}  // namespace Elysium::Services

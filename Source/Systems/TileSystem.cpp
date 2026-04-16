#include "Systems/TileSystem.h"
#include "Components/TileComponent.h"
#include "Core/Application.h"
#include "Core/Asset.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include <unordered_set>

namespace Elysium::Systems {

TileSystem::TileSystem(Context context) : System(context) {
}

void TileSystem::Update(float deltaTime) {
    if (initialized_) return;
    initialized_ = true;

    auto& assets = Application::GetInstance().GetService<Services::AssetService>();

    std::unordered_set<std::string> seen;
    world->Query<TileComponent>([&](Entity, const TileComponent& tile) {
        if (tile.tileName.empty()) return;
        if (seen.count(tile.tileName)) return;
        seen.insert(tile.tileName);
        LOG_DEBUGF("TileSystem", "Loading tile asset: %s", tile.tileName.c_str());
        assets.LoadAsset(AssetType::TILE, Path(tile.tileName));
    });

    LOG_INFOF("TileSystem", "Initialized — queued %d unique tile asset(s)", (int)seen.size());
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::TileSystem)

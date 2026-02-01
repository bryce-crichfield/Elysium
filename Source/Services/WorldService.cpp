#include "Services/WorldService.h"
#include <algorithm>
#include <iostream>
#include <set>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Entity.h"
#include "Services/AssetService.h"
#include "Services/SceneService.h"
#include "Services/ScriptService.h"
#include "imgui.h"
#include "raylib.h"

namespace Elysium::Services {

WorldService::WorldService() {
    name_ = "WorldService";
}

void WorldService::Initialize() {
    RegisterComponentTypes();
}

void WorldService::Shutdown() {
    // Service cleanup if needed
}

void WorldService::RegisterComponentTypes() {
    // Register components from the central registry
    const auto& inspectors = ComponentRegistry::Instance().GetInspectors();
    for (const auto& [name, inspectorFunc] : inspectors) {
        ComponentPlaceholder placeholder;
        placeholder.name = name;
        placeholder.drawFunc = [inspectorFunc](Entity e, Elysium::World* w) {
            inspectorFunc(w, e);
        };

        if (auto* access = ComponentRegistry::Instance().GetLuaAccess(name)) {
            placeholder.hasComponentFunc = [access](Entity e, Elysium::World* w) { return access->has(w, e); };
            placeholder.addComponentFunc = [access](Entity e, Elysium::World* w) { access->add(w, e); };
            placeholder.removeComponentFunc = [access](Entity e, Elysium::World* w) { access->remove(w, e); };
            placeholder.resetComponentFunc = [access](Entity e, Elysium::World* w) {
                access->remove(w, e);
                access->add(w, e);
            };
        }
        
        componentPlaceholders.push_back(placeholder);
    }

    // Register legacy components manually
    // All components migrated to registry.
}

void WorldService::Update(float deltaTime) {
    Profile;
    auto& app = Elysium::Application::GetInstance();
    auto& sceneService = app.GetService<SceneService>();
    auto newWorld = sceneService.GetScene() ? sceneService.GetScene()->GetWorld() : nullptr;

    if (newWorld != world) {
        world = newWorld;
    }

    // Auto-select dragged entities
    if (world) {
        world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
            if (bounds.isDragging) {
                selectedEntity = entity;
            }
        });
    }
}

}  // namespace Elysium::Services

#include "Services/EditorService.h"
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Entity.h"
#include "Services/SceneService.h"

namespace Elysium::Services {

EditorService::EditorService() {
    name_ = "EditorService";
}

void EditorService::Initialize() {
    RegisterComponentTypes();
}

void EditorService::Shutdown() {
}

void EditorService::RegisterComponentTypes() {
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

    std::sort(componentPlaceholders.begin(), componentPlaceholders.end(),
              [](const ComponentPlaceholder& a, const ComponentPlaceholder& b) {
                  return a.name < b.name;
              });
}

Elysium::World* EditorService::GetWorld() const {
    auto& app = Elysium::Application::GetInstance();
    auto& sceneService = app.GetService<SceneService>();
    auto* scene = sceneService.GetScene();
    return scene ? scene->GetWorld() : nullptr;
}

void EditorService::Update(float deltaTime) {
    Profile;

    auto* world = GetWorld();

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

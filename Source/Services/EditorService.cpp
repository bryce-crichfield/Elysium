#include "Services/EditorService.h"
#include <algorithm>
#include "Core/Common.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Entity.h"
#include "Interfaces/ISceneService.h"

namespace Elysium::Services {

EditorService::EditorService(ServiceLocator& registry) : registry_(registry) {}

void EditorService::Initialize() {
    RegisterComponentTypes();
}

void EditorService::Shutdown() {
}

Elysium::Scene* EditorService::GetInspectedScene() {
    auto& scenes = registry_.Get<ISceneService>();
    if (scenes.IsInStack(inspectedScene_)) return inspectedScene_;
    inspectedScene_ = nullptr;
    return scenes.GetTopScene();
}

void EditorService::RegisterComponentTypes() {
    const auto& inspectors = ComponentRegistry::Instance().GetInspectors();
    for (const auto& [name, inspectorFunc] : inspectors) {
        ComponentPlaceholder placeholder;
        placeholder.name = name;
        placeholder.drawFunc = [inspectorFunc, this](Entity e, Elysium::World* w) {
            inspectorFunc(w, e, registry_);
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

    // Inspector and Add Component list them in InspectorOrder, then by name.
    auto& registry = ComponentRegistry::Instance();
    std::sort(componentPlaceholders.begin(), componentPlaceholders.end(), [&](const auto& a, const auto& b) {
        const auto orderA = registry.GetInspectorOrder(a.name), orderB = registry.GetInspectorOrder(b.name);
        return orderA != orderB ? orderA < orderB : a.name < b.name;
    });
}

Elysium::World* EditorService::GetWorld() const {
    auto& sceneService = registry_.Get<ISceneService>();
    auto* scene = sceneService.GetTopScene();
    return scene ? scene->GetWorld() : nullptr;
}

void EditorService::SelectEntity(Entity entity, bool additive) {
    if (!additive) {
        selectedEntities_.clear();
    }
    if (entity != INVALID_ENTITY && !IsSelected(entity)) {
        selectedEntities_.push_back(entity);
    }
}

void EditorService::ClearSelection() {
    selectedEntities_.clear();
}

bool EditorService::IsSelected(Entity entity) const {
    return std::find(selectedEntities_.begin(), selectedEntities_.end(), entity) != selectedEntities_.end();
}

void EditorService::Update(float deltaTime) {
    Profile;

    // Auto-select dragged entities
    if (auto* world = GetWorld()) {
        world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
            if (bounds.isDragging) {
                SelectEntity(entity);
            }
        });
    }
}

}  // namespace Elysium::Services

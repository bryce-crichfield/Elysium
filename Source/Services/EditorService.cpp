#include "Services/EditorService.h"
#include <algorithm>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Entity.h"
#include "Core/Path.h"
#include "Core/Prefab.h"
#include "Core/World.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"

namespace Elysium::Services {

EditorService::EditorService(ServiceRegistry& registry) : Service(registry) {
    name_ = "EditorService";
}

EditorService::~EditorService() = default;

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
    if (prefabWorld_) {
        return prefabWorld_.get();
    }

    auto& app = Elysium::Application::GetInstance();
    auto& sceneService = app.GetService<SceneService>();
    auto* scene = sceneService.GetTopScene();
    return scene ? scene->GetWorld() : nullptr;
}

void EditorService::OpenPrefabForEditing(const std::string& relativePath) {
    std::string fullPath = Elysium::Path(relativePath).GetFullPath();

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* entitiesRoot = nullptr;
    if (!Elysium::LoadPrefabFile(fullPath, doc, entitiesRoot)) {
        return;
    }

    auto world = std::make_unique<Elysium::World>();
    // instanceId="" — load the prefab's own raw entities, unnamespaced, since we're
    // editing the source of truth rather than a placement of it.
    Elysium::LoadPrefabEntities(entitiesRoot, world.get(), "");

    prefabWorld_ = std::move(world);
    editingPrefabPath_ = relativePath;
    ClearSelection();
}

void EditorService::ClosePrefabEditing() {
    prefabWorld_.reset();
    editingPrefabPath_.clear();
    ClearSelection();
}

bool EditorService::SavePrefabEditing() {
    if (!prefabWorld_) return false;

    std::string fullPath = Elysium::Path(editingPrefabPath_).GetFullPath();
    if (!Elysium::SavePrefabFile(prefabWorld_.get(), fullPath)) {
        LOG_ERRORF("EditorService", "Failed to save prefab: %s", editingPrefabPath_.c_str());
        return false;
    }
    return true;
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

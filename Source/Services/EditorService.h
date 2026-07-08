#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Service.h"

namespace Elysium {
class World;
}  // namespace Elysium

namespace Elysium::Services {

struct ComponentPlaceholder {
    std::function<void(Entity, Elysium::World*)> drawFunc;
    std::function<bool(Entity, Elysium::World*)> hasComponentFunc;
    std::function<void(Entity, Elysium::World*)> addComponentFunc;
    std::function<void(Entity, Elysium::World*)> removeComponentFunc;
    std::function<void(Entity, Elysium::World*)> resetComponentFunc;
    std::string name;
};

// The editor's free/independent camera — decoupled from any in-scene CameraComponent so
// the viewport can be panned/zoomed around the scene without touching game state.
struct EditorCamera {
    Vector2 position = {0, 0};
    float zoom = 1.0f;
    // Set once the camera has been snapped to a sensible starting position (e.g. the first
    // real CameraComponent's transform) — avoids opening every scene centered on the origin.
    bool initialized = false;
};

class EditorService : public Elysium::Service {
   public:
    EditorService(ServiceRegistry& registry);
    ~EditorService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    Elysium::World* GetWorld() const;

    // Component introspection (used by WorldEditor)
    const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const { return componentPlaceholders; }

    // Selection — shared between WorldEditor's entity list/inspector and viewport picking.
    const std::vector<Entity>& GetSelectedEntities() const { return selectedEntities_; }
    void SelectEntity(Entity entity, bool additive = false);
    void ClearSelection();
    bool IsSelected(Entity entity) const;

    // The free camera RenderSystem renders through while in AppMode::Editor.
    EditorCamera& GetEditorCamera() { return editorCamera_; }

   private:
    std::vector<ComponentPlaceholder> componentPlaceholders;
    std::vector<Entity> selectedEntities_;
    EditorCamera editorCamera_;

    void RegisterComponentTypes();
};

}  // namespace Elysium::Services

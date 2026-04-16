#pragma once

#include <functional>
#include <limits>
#include <string>
#include <vector>
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Service.h"
#include "raylib.h"

namespace Elysium {
class Scene;
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

class EditorService : public Elysium::Service {
   public:
    EditorService();
    ~EditorService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Draw selection box + corner handles into the framebuffer (call inside BeginTextureMode)
    void RenderOverlay(float fbW, float fbH);

    Elysium::World* GetWorld() const;
    Elysium::Scene* GetTopScene() const;

    void SetSelectedEntity(Entity entity) { selectedEntity_ = entity; isDragging_ = false; }
    Entity GetSelectedEntity() const { return selectedEntity_; }

    // Component introspection (used by WorldEditor)
    const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const { return componentPlaceholders_; }

   private:
    // AABB of the currently selected entity (world or screen space)
    struct EntityAABB {
        Vector2 min = {0, 0};
        Vector2 max = {0, 0};
        bool isScreenSpace = false;
    };

    void ComputeSelectedAABB(Elysium::World* world, Elysium::Scene* scene);
    Entity GetCameraEntity(Elysium::World* world) const;
    Vector2 FbToWorld(Vector2 fbPos, Elysium::World* world, Entity cameraEntity) const;
    void ProcessViewportInput(Elysium::World* world, Elysium::Scene* scene, float deltaTime);

    void RegisterComponentTypes();

    Entity selectedEntity_ = INVALID_ENTITY;
    EntityAABB selectedAABB_;
    bool selectedAABBValid_ = false;

    bool isDragging_  = false;
    bool isPanning_   = false;
    bool hasDragged_  = false;   // true once mouse moves past drag threshold after press
    Vector2 pressPos_ = {0, 0};  // framebuffer position at mouse-down

    std::vector<ComponentPlaceholder> componentPlaceholders_;
};

}  // namespace Elysium::Services

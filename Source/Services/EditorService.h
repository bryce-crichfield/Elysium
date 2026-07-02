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

class EditorService : public Elysium::Service {
   public:
    EditorService(ServiceRegistry& registry);
    ~EditorService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    Elysium::World* GetWorld() const;
    void SetSelectedEntity(Entity entity) { selectedEntity = entity; }
    Entity GetSelectedEntity() const { return selectedEntity; }

    // Component introspection (used by WorldEditor)
    const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const { return componentPlaceholders; }

   private:
    Entity selectedEntity = INVALID_ENTITY;

    std::vector<ComponentPlaceholder> componentPlaceholders;

    void RegisterComponentTypes();
};

}  // namespace Elysium::Services

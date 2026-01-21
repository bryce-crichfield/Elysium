#pragma once

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Service.h"

namespace Elysium {
class World;
class Scene;
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

class WorldService : public Elysium::Service {
   public:
    WorldService();
    ~WorldService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Service-specific functionality
    void SetCurrentWorld(Elysium::World* w) {
        currentWorld = w;
        world = w;
    }
    Elysium::World* GetWorld() const { return world; }
    void SetSelectedEntity(Entity entity) { selectedEntity = entity; }
    Entity GetSelectedEntity() const { return selectedEntity; }

    // Component introspection (used by WorldEditor)
    const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const { return componentPlaceholders; }

   private:
    Entity selectedEntity = INVALID_ENTITY;
    Elysium::World* currentWorld = nullptr;
    Elysium::World* world = nullptr;

    std::vector<ComponentPlaceholder> componentPlaceholders;

    void RegisterComponentTypes();

    template <typename T>
    void RegisterComponent(const std::string& name);

    template <typename T>
    void DrawComponent(Entity entity, Elysium::World* world);
};

template <typename T>
void WorldService::RegisterComponent(const std::string& name) {
    ComponentPlaceholder placeholder;
    placeholder.name = name;

    placeholder.drawFunc = [this](Entity entity, Elysium::World* world) { this->DrawComponent<T>(entity, world); };
    placeholder.hasComponentFunc = [](Entity entity, Elysium::World* world) { return world->HasComponent<T>(entity); };
    placeholder.addComponentFunc = [](Entity entity, Elysium::World* world) { world->AddComponent<T>(entity, T{}); };
    placeholder.removeComponentFunc = [](Entity entity, Elysium::World* world) { world->RemoveComponent<T>(entity); };
    placeholder.resetComponentFunc = [](Entity entity, Elysium::World* world) {
        world->RemoveComponent<T>(entity);
        world->AddComponent<T>(entity, T{});
    };

    componentPlaceholders.push_back(placeholder);
}

}  // namespace Elysium::Services

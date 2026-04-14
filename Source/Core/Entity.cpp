#include "Entity.h"
#include "Sprite.h"
#include "ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/World.h"
#include "Components/ParentComponent.h"

namespace Elysium {
void ComponentManager::EntityDestroyed(Entity entity) {
    for (auto const& pair : componentArrays) {
        auto const& component = pair.second;
        component->EntityDestroyed(entity);
    }
}

void ComponentManager::CloneAllComponents(Entity source, Entity dest) {
    for (auto const& pair : componentArrays) {
        auto const& component = pair.second;
        component->CloneComponent(source, dest);
    }
}

// EntityManager implementations
EntityManager::EntityManager() {
    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
        availableEntities.push(entity);
    }
    componentMasks.resize(MAX_ENTITIES);
    livingEntities.reserve(1000);
}

Entity EntityManager::CreateEntity() {
    if (livingEntityCount >= MAX_ENTITIES) {
        return INVALID_ENTITY;
    }
    Entity id = availableEntities.front();
    availableEntities.pop();
    livingEntities.push_back(id);
    ++livingEntityCount;
    return id;
}

void EntityManager::DestroyEntity(Entity entity) {
    if (entity >= MAX_ENTITIES || livingEntityCount == 0) {
        return;
    }
    componentMasks[entity].reset();
    availableEntities.push(entity);
    auto it = std::find(livingEntities.begin(), livingEntities.end(), entity);
    if (it != livingEntities.end()) {
        std::swap(*it, livingEntities.back());
        livingEntities.pop_back();
        --livingEntityCount;
    }
}

void EntityManager::SetComponentMask(Entity entity, ComponentMask mask) {
    if (entity < MAX_ENTITIES) {
        componentMasks[entity] = mask;
    }
}

ComponentMask EntityManager::GetComponentMask(Entity entity) {
    if (entity < MAX_ENTITIES) {
        return componentMasks[entity];
    }
    return ComponentMask();
}

size_t EntityManager::GetLivingEntityCount() const {
    return livingEntityCount;
}

const std::vector<Entity>& EntityManager::GetLivingEntities() const {
    return livingEntities;
}

// World implementations
World::World() {
    componentManager = std::make_unique<ComponentManager>();
    entityManager = std::make_unique<EntityManager>();

    ComponentRegistry::Instance().RegisterAllComponents(*this);

    // RegisterComponent<NameComponent>(); // Handled by Registry
    // RegisterComponent<PositionComponent>(); // Handled by Registry
    RegisterComponent<ScaleComponent>();
    RegisterComponent<MovementComponent>();
    RegisterComponent<LayerComponent>();
    RegisterComponent<LightComponent>();
    RegisterComponent<RectangleComponent>();
    RegisterComponent<CircleComponent>();
    RegisterComponent<SpriteComponent>();
    RegisterComponent<TextComponent>();
    RegisterComponent<CameraComponent>();
    RegisterComponent<FollowComponent>();
    RegisterComponent<TileComponent>();
    RegisterComponent<CooldownComponent>();
    RegisterComponent<UnitComponent>();
    RegisterComponent<BoundsComponent>();
    RegisterComponent<SelectionComponent>();
    RegisterComponent<ScriptComponent>();
    RegisterComponent<KinematicsComponent>();
    RegisterComponent<HealthComponent>();
    RegisterComponent<AttackComponent>();
}

void World::AddWorldListener(IWorldListener* listener) {
    worldListeners_.push_back(listener);
}

void World::RemoveWorldListener(IWorldListener* listener) {
    worldListeners_.erase(
        std::remove(worldListeners_.begin(), worldListeners_.end(), listener),
        worldListeners_.end());
}

Entity World::CreateEntity() {
    Entity entity = entityManager->CreateEntity();
    for (auto* listener : worldListeners_) {
        listener->OnEntityCreated(entity);
    }
    return entity;
}

Entity World::CloneEntity(Entity source) {
    // Create new entity directly (don't use CreateEntity() to avoid double notification)
    Entity newEntity = entityManager->CreateEntity();

    // Copy component mask
    ComponentMask mask = entityManager->GetComponentMask(source);
    entityManager->SetComponentMask(newEntity, mask);

    // Clone all components
    componentManager->CloneAllComponents(source, newEntity);

    // Special handling for ScriptComponent: Reset initialization flag
    if (HasComponent<ScriptComponent>(newEntity)) {
        auto& script = GetComponent<ScriptComponent>(newEntity);
        std::fill(script.isInitialized.begin(), script.isInitialized.end(), false);
    }

    // Notify listeners after clone is complete
    for (auto* listener : worldListeners_) {
        listener->OnEntityCreated(newEntity);
    }

    return newEntity;
}

bool World::GetEntityByName(std::string name, Entity* entity) {
    bool found = false;
    Query<NameComponent>([&](Entity e, auto& nameComp) {
        if (!found && nameComp.name == name) {
            *entity = e;
            found = true;
        }
    });
    return found;
}

std::string World::GetEntityName(Entity entity) const {
    if (HasComponent<NameComponent>(entity)) {
        return GetComponent<NameComponent>(entity).name;
    }
    return "";
}

void World::AddChild(Entity parent, Entity child) {
    // Record in adjacency map
    childrenMap_[parent].push_back(child);

    // Stamp ParentComponent on the child so it knows its parent
    if (!HasComponent<ParentComponent>(child)) {
        AddComponent<ParentComponent>(child, ParentComponent{});
    }
    auto& pc = GetComponent<ParentComponent>(child);
    pc.parent = parent;

    // Keep targetName in sync so save/load round-trips correctly
    if (pc.targetName.empty()) {
        pc.targetName = GetEntityName(parent);
    }
}

void World::RemoveChild(Entity parent, Entity child) {
    auto it = childrenMap_.find(parent);
    if (it != childrenMap_.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), child), vec.end());
        if (vec.empty()) {
            childrenMap_.erase(it);
        }
    }

    if (HasComponent<ParentComponent>(child)) {
        auto& pc = GetComponent<ParentComponent>(child);
        pc.parent = INVALID_ENTITY;
    }
}

static const std::vector<Entity> s_emptyChildren;

const std::vector<Entity>& World::GetChildren(Entity parent) const {
    auto it = childrenMap_.find(parent);
    if (it != childrenMap_.end()) {
        return it->second;
    }
    return s_emptyChildren;
}

Entity World::GetParent(Entity child) const {
    if (HasComponent<ParentComponent>(child)) {
        return GetComponent<ParentComponent>(child).parent;
    }
    return INVALID_ENTITY;
}

void World::DestroyEntity(Entity entity) {
    // Notify listeners BEFORE destruction so they can still access components
    for (auto* listener : worldListeners_) {
        listener->OnEntityDestroyed(entity);
    }

    // Hierarchy cleanup: detach from parent and orphan any children
    if (HasComponent<ParentComponent>(entity)) {
        Entity parent = GetComponent<ParentComponent>(entity).parent;
        if (parent != INVALID_ENTITY) {
            RemoveChild(parent, entity);
        }
    }
    auto childIt = childrenMap_.find(entity);
    if (childIt != childrenMap_.end()) {
        // Clear each child's parent reference without calling RemoveChild
        // (which would mutate the vector we're iterating)
        for (Entity child : childIt->second) {
            if (HasComponent<ParentComponent>(child)) {
                GetComponent<ParentComponent>(child).parent = INVALID_ENTITY;
            }
        }
        childrenMap_.erase(childIt);
    }

    componentManager->EntityDestroyed(entity);
    entityManager->DestroyEntity(entity);
}

size_t World::GetEntityCount() const {
    return entityManager->GetLivingEntityCount();
}

const std::vector<Entity>& World::GetLivingEntities() const {
    return entityManager->GetLivingEntities();
}
}  // namespace Elysium

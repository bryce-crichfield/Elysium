#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <queue>
#include <typeindex>
#include <bitset>
#include "raylib.h"
#include <stdexcept>
#include <algorithm>
#include "raylib.h"
#include "Component.h"

// Entity-Component-System (ECS) Storage Layout:
//
// Entity: Just an ID (size_t). Entities are managed by EntityManager with component masks.
//
// Components: Stored in separate TypedComponentArray<T> for each component type.
// Each array is dense (no gaps), with entity->index mapping for O(1) access.
//
// ComponentManager: Holds heterogeneous component arrays via ComponentArray interface.
// Uses type erasure to store TypedComponentArray<T> instances in a single map.
//
// World: Top-level API that orchestrates EntityManager + ComponentManager.
// Provides Query<Components...>() for efficient iteration over entities with specific components.

namespace Elysium
{
using Entity = size_t;
constexpr size_t MAX_ENTITIES = 10000;
constexpr size_t MAX_COMPONENTS = 32;
constexpr Entity INVALID_ENTITY = MAX_ENTITIES;

using ComponentMask = std::bitset<MAX_COMPONENTS>;

class ComponentArray
{
public:
    virtual ~ComponentArray() = default;
    virtual void EntityDestroyed(Entity entity) = 0;
};

template<typename T>
class TypedComponentArray : public ComponentArray
{
private:
    std::vector<T> componentArray;
    std::unordered_map<Entity, size_t> entityToIndex;
    std::unordered_map<size_t, Entity> indexToEntity;
    size_t size = 0;

public:
    void InsertData(Entity entity, T component);
    void RemoveData(Entity entity);
    T& GetData(Entity entity);
    bool HasData(Entity entity) const;
    void EntityDestroyed(Entity entity) override;
    std::vector<T>& GetArray();
    const std::vector<T>& GetArray() const;
    std::unordered_map<Entity, size_t>& GetEntityToIndex();
    const std::unordered_map<Entity, size_t>& GetEntityToIndex() const;
};

class ComponentManager
{
private:
    std::unordered_map<std::type_index, std::shared_ptr<ComponentArray>> componentArrays;
    std::unordered_map<std::type_index, size_t> componentTypes;
    size_t nextComponentType = 0;

public:
    template<typename T>
    void RegisterComponent();

    template<typename T>
    size_t GetComponentType() const;

    template<typename T>
    void AddComponent(Entity entity, T component);

    template<typename T>
    void RemoveComponent(Entity entity);

    template<typename T>
    T& GetComponent(Entity entity);

    template<typename T>
    bool HasComponent(Entity entity) const;

    void EntityDestroyed(Entity entity);

    template<typename T>
    std::vector<T>& GetRawComponentArray();

    template<typename T>
    std::unordered_map<Entity, size_t>& GetEntityToIndexMap();

    template<typename T>
    std::shared_ptr<TypedComponentArray<T>> GetComponentArray();

    template<typename T>
    std::shared_ptr<const TypedComponentArray<T>> GetComponentArray() const;
};

class EntityManager
{
private:
    std::queue<Entity, std::deque<Entity>> availableEntities;
    std::vector<ComponentMask> componentMasks;
    std::vector<Entity> livingEntities;
    size_t livingEntityCount = 0;

public:
    EntityManager();
    Entity CreateEntity();
    void DestroyEntity(Entity entity);
    void SetComponentMask(Entity entity, ComponentMask mask);
    ComponentMask GetComponentMask(Entity entity);
    size_t GetLivingEntityCount() const;
    const std::vector<Entity>& GetLivingEntities() const;
};

class World
{
private:
    std::unique_ptr<ComponentManager> componentManager;
    std::unique_ptr<EntityManager> entityManager;

public:
    World(); // Declaration only
    ~World() = default;
    Entity CreateEntity(); // Declaration only
    void DestroyEntity(Entity entity); // Declaration only
    size_t GetEntityCount() const; // Declaration only
    const std::vector<Entity>& GetLivingEntities() const; // Declaration only

    // Template methods remain in the header
    template<typename T>
    void RegisterComponent();

    template<typename T>
    void AddComponent(Entity entity, T component);

    template<typename T>
    void RemoveComponent(Entity entity);

    template<typename T>
    T& GetComponent(Entity entity);

    template<typename T>
    const T& GetComponent(Entity entity) const;

    template<typename T>
    bool HasComponent(Entity entity) const;

    template<typename T>
    size_t GetComponentType() const;

    template<typename... ComponentTypes>
    const std::vector<Entity>& GetEntitiesWithComponents() const;

    template<typename... ComponentTypes, typename Func>
    void Query(Func&& func);

    bool GetEntityByName(std::string name, Entity *entity);
    std::string GetEntityName(Entity entity) const;
};

// TypedComponentArray implementations
template<typename T>
void TypedComponentArray<T>::InsertData(Entity entity, T component)
{
    if (entityToIndex.find(entity) != entityToIndex.end()) {
        return; // Component already exists
    }

    size_t newIndex = size;
    entityToIndex[entity] = newIndex;
    indexToEntity[newIndex] = entity;

    if (componentArray.size() <= newIndex) {
        componentArray.resize(newIndex + 1);
    }
    componentArray[newIndex] = component;
    ++size;
}

template<typename T>
void TypedComponentArray<T>::RemoveData(Entity entity)
{
    if (entityToIndex.find(entity) == entityToIndex.end()) {
        return; // Component doesn't exist
    }

    size_t indexOfRemovedEntity = entityToIndex[entity];
    size_t indexOfLastElement = size - 1;
    componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

    Entity entityOfLastElement = indexToEntity[indexOfLastElement];
    entityToIndex[entityOfLastElement] = indexOfRemovedEntity;
    indexToEntity[indexOfRemovedEntity] = entityOfLastElement;

    entityToIndex.erase(entity);
    indexToEntity.erase(indexOfLastElement);
    --size;
}

template<typename T>
T& TypedComponentArray<T>::GetData(Entity entity)
{
    auto it = entityToIndex.find(entity);
    if (it == entityToIndex.end()) {
        throw std::runtime_error("Entity does not have this component");
    }
    return componentArray[it->second];
}

template<typename T>
bool TypedComponentArray<T>::HasData(Entity entity) const
{
    return entityToIndex.find(entity) != entityToIndex.end();
}

template<typename T>
void TypedComponentArray<T>::EntityDestroyed(Entity entity)
{
    if (entityToIndex.find(entity) != entityToIndex.end()) {
        RemoveData(entity);
    }
}

template<typename T>
std::vector<T>& TypedComponentArray<T>::GetArray()
{
    return componentArray;
}

template<typename T>
const std::vector<T>& TypedComponentArray<T>::GetArray() const
{
    return componentArray;
}

template<typename T>
std::unordered_map<Entity, size_t>& TypedComponentArray<T>::GetEntityToIndex()
{
    return entityToIndex;
}

template<typename T>
const std::unordered_map<Entity, size_t>& TypedComponentArray<T>::GetEntityToIndex() const
{
    return entityToIndex;
}

template<typename T>
std::shared_ptr<TypedComponentArray<T>> ComponentManager::GetComponentArray()
{
    std::type_index typeName = std::type_index(typeid(T));
    return std::static_pointer_cast<TypedComponentArray<T>>(componentArrays[typeName]);
}

template<typename T>
std::shared_ptr<const TypedComponentArray<T>> ComponentManager::GetComponentArray() const
{
    std::type_index typeName = std::type_index(typeid(T));
    auto it = componentArrays.find(typeName);
    if (it == componentArrays.end()) {
        throw std::runtime_error("Component type not registered");
    }
    return std::static_pointer_cast<const TypedComponentArray<T>>(it->second);
}

template<typename T>
void ComponentManager::RegisterComponent()
{
    std::type_index typeName = std::type_index(typeid(T));
    componentTypes[typeName] = nextComponentType;
    componentArrays[typeName] = std::make_shared<TypedComponentArray<T>>();
    ++nextComponentType;
}

template<typename T>
size_t ComponentManager::GetComponentType() const
{
    std::type_index typeName = std::type_index(typeid(T));
    return componentTypes.at(typeName);
}

template<typename T>
void ComponentManager::AddComponent(Entity entity, T component)
{
    GetComponentArray<T>()->InsertData(entity, component);
}

template<typename T>
void ComponentManager::RemoveComponent(Entity entity)
{
    GetComponentArray<T>()->RemoveData(entity);
}

template<typename T>
T& ComponentManager::GetComponent(Entity entity)
{
    return GetComponentArray<T>()->GetData(entity);
}

template<typename T>
bool ComponentManager::HasComponent(Entity entity) const
{
    return GetComponentArray<T>()->HasData(entity);
}

template<typename T>
std::vector<T>& ComponentManager::GetRawComponentArray()
{
    return GetComponentArray<T>()->GetArray();
}

template<typename T>
std::unordered_map<Entity, size_t>& ComponentManager::GetEntityToIndexMap()
{
    return GetComponentArray<T>()->GetEntityToIndex();
}

template<typename T>
void World::RegisterComponent()
{
    componentManager->RegisterComponent<T>();
}

template<typename T>
void World::AddComponent(Entity entity, T component)
{
    componentManager->AddComponent<T>(entity, component);

    auto mask = entityManager->GetComponentMask(entity);
    mask.set(componentManager->GetComponentType<T>());
    entityManager->SetComponentMask(entity, mask);
}

template<typename T>
void World::RemoveComponent(Entity entity)
{
    componentManager->RemoveComponent<T>(entity);

    auto mask = entityManager->GetComponentMask(entity);
    mask.reset(componentManager->GetComponentType<T>());
    entityManager->SetComponentMask(entity, mask);
}

template<typename T>
T& World::GetComponent(Entity entity)
{
    return componentManager->GetComponent<T>(entity);
}

template<typename T>
const T& World::GetComponent(Entity entity) const
{
    auto componentArray = componentManager->GetComponentArray<T>();
    auto it = componentArray->GetEntityToIndex().find(entity);
    if (it == componentArray->GetEntityToIndex().end()) {
        throw std::runtime_error("Entity does not have this component");
    }
    return componentArray->GetArray()[it->second];
}

template<typename T>
bool World::HasComponent(Entity entity) const
{
    return componentManager->HasComponent<T>(entity);
}

template<typename T>
size_t World::GetComponentType() const
{
    return componentManager->GetComponentType<T>();
}

template<typename... ComponentTypes>
const std::vector<Entity>& World::GetEntitiesWithComponents() const
{
    ComponentMask mask;
    (mask.set(GetComponentType<ComponentTypes>()), ...);

    // Use a static thread_local buffer to avoid allocation overhead
    static thread_local std::vector<Entity> entities;
    entities.clear();
    entities.reserve(entityManager->GetLivingEntityCount());

    // Only iterate through living entities instead of all possible entities
    const auto& livingEntities = entityManager->GetLivingEntities();
    for (Entity entity : livingEntities) {
        if ((entityManager->GetComponentMask(entity) & mask) == mask) {
            entities.push_back(entity);
        }
    }
    return entities;
}

template<typename... ComponentTypes, typename Func>
void World::Query(Func&& func)
{
    ComponentMask mask;
    (mask.set(GetComponentType<ComponentTypes>()), ...);

    // Only iterate through living entities instead of all possible entities
    const auto& livingEntities = entityManager->GetLivingEntities();
    for (Entity entity : livingEntities) {
        if ((entityManager->GetComponentMask(entity) & mask) == mask) {
            // Unpack components as references and pass to lambda
            // First parameter is entity, then components
            func(entity, GetComponent<ComponentTypes>(entity)...);
        }
    }
}
};

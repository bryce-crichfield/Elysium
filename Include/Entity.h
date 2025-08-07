#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <queue>
#include <typeindex>
#include <typeinfo>
#include <bitset>
#include <algorithm>
#include <stdexcept>
#include <functional>
#include "raylib.h"
#include "Services/MemoryTracker.h"

using Entity = size_t;
constexpr size_t MAX_ENTITIES = 10000;
constexpr size_t MAX_COMPONENTS = 32;
constexpr Entity INVALID_ENTITY = MAX_ENTITIES;

using ComponentMask = std::bitset<MAX_COMPONENTS>;

// Type aliases for cleaner template declarations
template<typename T>
using TrackedVector = std::vector<T, Elysium::Services::TrackedAllocator<T>>;

template<typename Key, typename Value>
using TrackedUnorderedMap = std::unordered_map<
    Key, Value,
    std::hash<Key>,
    std::equal_to<Key>,
    Elysium::Services::TrackedAllocator<std::pair<const Key, Value>>
>;

template<typename T>
using TrackedDeque = std::deque<T, Elysium::Services::TrackedAllocator<T>>;

namespace Elysium { struct Action; }


struct AnimationComponent {
    std::queue<std::shared_ptr<Elysium::Action>> actionQueue;
    std::shared_ptr<Elysium::Action> currentAction = nullptr;
};

struct PositionComponent {
    float x, y;
    PositionComponent(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}
};

struct VelocityComponent {
    float x, y;
    VelocityComponent(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}
};

struct CircleRenderComponent {
    float radius;
    Color color;
    bool drawOutline;
    Color outlineColor;

    CircleRenderComponent(float r = 10.0f, Color c = WHITE, bool outline = true, Color outlineC = BLACK)
        : radius(r), color(c), drawOutline(outline), outlineColor(outlineC) {}
};

struct PhysicsComponent {
    float mass;
    float restitution; // bounciness (0-1)
    float friction;
    bool affectedByGravity;

    PhysicsComponent(float m = 1.0f, float rest = 0.8f, float fric = 0.1f, bool gravity = true)
        : mass(m), restitution(rest), friction(fric), affectedByGravity(gravity) {}
};

struct SpriteComponent {
    std::string textureName;
    std::string frame;
    Vector2 scale;
    float rotation;
    Color tint;

    SpriteComponent(const std::string& name = "", const std::string& f = "", Vector2 s = {1.0f, 1.0f}, float rot = 0.0f, Color t = WHITE)
        : textureName(name), frame(f), scale(s), rotation(rot), tint(t) {}
};

struct TextComponent {
    std::string content;
    int fontSize;
    Color color;

    TextComponent(const std::string& text = "", int size = 20, Color c = BLACK)
        : content(text), fontSize(size), color(c) {}
};

class ComponentArray {
public:
    virtual ~ComponentArray() = default;
    virtual void EntityDestroyed(Entity entity) = 0;
};

template<typename T>
class TypedComponentArray : public ComponentArray {
private:
    TrackedVector<T> componentArray;
    TrackedUnorderedMap<Entity, size_t> entityToIndex;
    TrackedUnorderedMap<size_t, Entity> indexToEntity;
    size_t size = 0;

public:
    void InsertData(Entity entity, T component) {
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

    void RemoveData(Entity entity) {
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

    T& GetData(Entity entity) {
        auto it = entityToIndex.find(entity);
        if (it == entityToIndex.end()) {
            throw std::runtime_error("Entity does not have this component");
        }
        return componentArray[it->second];
    }

    bool HasData(Entity entity) const {
        return entityToIndex.find(entity) != entityToIndex.end();
    }

    void EntityDestroyed(Entity entity) override {
        if (entityToIndex.find(entity) != entityToIndex.end()) {
            RemoveData(entity);
        }
    }

    TrackedVector<T>& GetArray() { return componentArray; }
    const TrackedVector<T>& GetArray() const { return componentArray; }
    TrackedUnorderedMap<Entity, size_t>& GetEntityToIndex() { return entityToIndex; }
    const TrackedUnorderedMap<Entity, size_t>& GetEntityToIndex() const { return entityToIndex; }
};

class ComponentManager {
private:
    TrackedUnorderedMap<std::type_index, std::shared_ptr<ComponentArray>> componentArrays;
    TrackedUnorderedMap<std::type_index, size_t> componentTypes;
    size_t nextComponentType = 0;

    template<typename T>
    std::shared_ptr<TypedComponentArray<T>> GetComponentArray() {
        std::type_index typeName = std::type_index(typeid(T));
        return std::static_pointer_cast<TypedComponentArray<T>>(componentArrays[typeName]);
    }

    template<typename T>
    std::shared_ptr<const TypedComponentArray<T>> GetComponentArray() const {
        std::type_index typeName = std::type_index(typeid(T));
        auto it = componentArrays.find(typeName);
        if (it == componentArrays.end()) {
            throw std::runtime_error("Component type not registered");
        }
        return std::static_pointer_cast<const TypedComponentArray<T>>(it->second);
    }

public:
    template<typename T>
    void RegisterComponent() {
        std::type_index typeName = std::type_index(typeid(T));
        componentTypes[typeName] = nextComponentType;
        componentArrays[typeName] = std::make_shared<TypedComponentArray<T>>();
        ++nextComponentType;
    }

    template<typename T>
    size_t GetComponentType() const {
        std::type_index typeName = std::type_index(typeid(T));
        return componentTypes.at(typeName);
    }

    template<typename T>
    void AddComponent(Entity entity, T component) {
        GetComponentArray<T>()->InsertData(entity, component);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        GetComponentArray<T>()->RemoveData(entity);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        return GetComponentArray<T>()->GetData(entity);
    }

    template<typename T>
    bool HasComponent(Entity entity) const {
        return GetComponentArray<T>()->HasData(entity);
    }

    void EntityDestroyed(Entity entity) {
        for (auto const& pair : componentArrays) {
            auto const& component = pair.second;
            component->EntityDestroyed(entity);
        }
    }

    template<typename T>
    TrackedVector<T>& GetRawComponentArray() {
        return GetComponentArray<T>()->GetArray();
    }

    template<typename T>
    TrackedUnorderedMap<Entity, size_t>& GetEntityToIndexMap() {
        return GetComponentArray<T>()->GetEntityToIndex();
    }
};

class EntityManager {
private:
    std::queue<Entity, TrackedDeque<Entity>> availableEntities;
    std::unordered_map<Entity, std::string> names;
    TrackedVector<ComponentMask> componentMasks;
    TrackedVector<Entity> livingEntities; // Track living entities for fast iteration
    size_t livingEntityCount = 0;

public:
    EntityManager() {
        for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
            availableEntities.push(entity);
        }
        componentMasks.resize(MAX_ENTITIES);
        livingEntities.reserve(1000); // Reserve space for living entities
    }

    Entity CreateEntity() {
        if (livingEntityCount >= MAX_ENTITIES) {
            return INVALID_ENTITY;
        }

        Entity id = availableEntities.front();
        availableEntities.pop();
        livingEntities.push_back(id);
        ++livingEntityCount;
        return id;
    }

    Entity CreateEntity(std::string name) {
        Entity entity = CreateEntity();
        if (entity != INVALID_ENTITY) {
            names[entity] = name;
        }
        return entity;
    }

    void DestroyEntity(Entity entity) {
        if (entity >= MAX_ENTITIES || livingEntityCount == 0) {
            return;
        }

        componentMasks[entity].reset();
        availableEntities.push(entity);

        // Remove from living entities list
        auto it = std::find(livingEntities.begin(), livingEntities.end(), entity);
        if (it != livingEntities.end()) {
            std::swap(*it, livingEntities.back());
            livingEntities.pop_back();
            --livingEntityCount;
        }

        // Remove from names map if it exists
        auto nameIt = names.find(entity);
        if (nameIt != names.end()) {
            names.erase(nameIt);
        }
    }

    bool GetEntityByName(std::string name, Entity* entity) {
        for (const auto& pair : names) {
            if (pair.second == name) {
                *entity = pair.first;
                return true;
            }
        }
        return false;
    }

    void SetComponentMask(Entity entity, ComponentMask mask) {
        if (entity < MAX_ENTITIES) {
            componentMasks[entity] = mask;
        }
    }

    ComponentMask GetComponentMask(Entity entity) {
        if (entity < MAX_ENTITIES) {
            return componentMasks[entity];
        }
        return ComponentMask();
    }

    size_t GetLivingEntityCount() const { return livingEntityCount; }

    const TrackedVector<Entity>& GetLivingEntities() const {
        return livingEntities;
    }
};

class EntityWorld {
private:
    std::unique_ptr<ComponentManager> componentManager;
    std::unique_ptr<EntityManager> entityManager;


    // Pre-allocated vectors for entity queries to avoid frequent allocations
    mutable TrackedVector<Entity> queryBuffer1_;
    mutable TrackedVector<Entity> queryBuffer2_;
    mutable TrackedVector<Entity> queryBuffer3_;
    mutable size_t currentQueryBuffer_ = 0;

public:
    EntityWorld() {
        componentManager = std::make_unique<ComponentManager>();
        entityManager = std::make_unique<EntityManager>();

        // Pre-allocate query buffers
        queryBuffer1_.reserve(1000);
        queryBuffer2_.reserve(1000);
        queryBuffer3_.reserve(1000);

        RegisterComponent<PositionComponent>();
        RegisterComponent<VelocityComponent>();
        RegisterComponent<CircleRenderComponent>();
        RegisterComponent<PhysicsComponent>();
        RegisterComponent<SpriteComponent>();
        RegisterComponent<TextComponent>();
        RegisterComponent<AnimationComponent>();
    }

    Entity CreateEntity() {
        return entityManager->CreateEntity();
    }

    Entity CreateEntity(std::string name) {
        return entityManager->CreateEntity(name);
    }

    bool GetEntityByName(std::string name, Entity* entity) {
        return entityManager->GetEntityByName(name, entity);
    }

    void DestroyEntity(Entity entity) {
        entityManager->DestroyEntity(entity);
        componentManager->EntityDestroyed(entity);
    }

    template<typename T>
    void RegisterComponent() {
        componentManager->RegisterComponent<T>();
    }

    template<typename T>
    void AddComponent(Entity entity, T component) {
        componentManager->AddComponent<T>(entity, component);

        auto mask = entityManager->GetComponentMask(entity);
        mask.set(componentManager->GetComponentType<T>());
        entityManager->SetComponentMask(entity, mask);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        componentManager->RemoveComponent<T>(entity);

        auto mask = entityManager->GetComponentMask(entity);
        mask.reset(componentManager->GetComponentType<T>());
        entityManager->SetComponentMask(entity, mask);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        return componentManager->GetComponent<T>(entity);
    }

    template<typename T>
    const T& GetComponent(Entity entity) const {
        auto componentArray = componentManager->GetComponentArray<T>();
        auto it = componentArray->GetEntityToIndex().find(entity);
        if (it == componentArray->GetEntityToIndex().end()) {
            throw std::runtime_error("Entity does not have this component");
        }
        return componentArray->GetArray()[it->second];
    }

    template<typename T>
    bool HasComponent(Entity entity) const {
        return componentManager->HasComponent<T>(entity);
    }

    template<typename T>
    size_t GetComponentType() const {
        return componentManager->GetComponentType<T>();
    }

    template<typename... ComponentTypes>
    const TrackedVector<Entity>& GetEntitiesWithComponents() const {
        ComponentMask mask;
        (mask.set(GetComponentType<ComponentTypes>()), ...);

        // Rotate through buffers to avoid conflicts when multiple systems query simultaneously
        auto& entities = (currentQueryBuffer_ == 0) ? queryBuffer1_ :
                        (currentQueryBuffer_ == 1) ? queryBuffer2_ : queryBuffer3_;
        currentQueryBuffer_ = (currentQueryBuffer_ + 1) % 3;

        entities.clear();
        entities.reserve(entityManager->GetLivingEntityCount()); // Pre-allocate based on living entities

        // Only iterate through living entities instead of all possible entities
        const auto& livingEntities = entityManager->GetLivingEntities();
        for (Entity entity : livingEntities) {
            if ((entityManager->GetComponentMask(entity) & mask) == mask) {
                entities.push_back(entity);
            }
        }
        return entities;
    }

    // Iterator-based version for even better performance
    template<typename... ComponentTypes, typename Func>
    void ForEachEntityWith(Func&& func) const {
        ComponentMask mask;
        (mask.set(GetComponentType<ComponentTypes>()), ...);

        // Only iterate through living entities instead of all possible entities
        const auto& livingEntities = entityManager->GetLivingEntities();
        for (Entity entity : livingEntities) {
            if ((entityManager->GetComponentMask(entity) & mask) == mask) {
                func(entity);
            }
        }
    }

    size_t GetEntityCount() const {
        return entityManager->GetLivingEntityCount();
    }

};

class System {
protected:
    EntityWorld* world;

public:
    System(EntityWorld* w) : world(w) {}
    virtual ~System() = default;

    virtual void Update(float deltaTime) = 0;
    virtual void Render() {}
    virtual void OnEvent(const char* eventName, void* data) {}
};

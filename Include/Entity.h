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
#include "raylib.h"
#include "Services/MemoryTracker.h"

using Entity = size_t;
constexpr size_t MAX_ENTITIES = 10000;
constexpr size_t MAX_COMPONENTS = 32;

using ComponentMask = std::bitset<MAX_COMPONENTS>;

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
    std::vector<T, Elysium::Services::TrackedAllocator<T>> componentArray;
    std::unordered_map<Entity, size_t, std::hash<Entity>, std::equal_to<Entity>, Elysium::Services::TrackedAllocator<std::pair<const Entity, size_t>>> entityToIndex;
    std::unordered_map<size_t, Entity, std::hash<size_t>, std::equal_to<size_t>, Elysium::Services::TrackedAllocator<std::pair<const size_t, Entity>>> indexToEntity;
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
        return componentArray[entityToIndex[entity]];
    }
    
    bool HasData(Entity entity) const {
        return entityToIndex.find(entity) != entityToIndex.end();
    }
    
    void EntityDestroyed(Entity entity) override {
        if (entityToIndex.find(entity) != entityToIndex.end()) {
            RemoveData(entity);
        }
    }
    
    std::vector<T, Elysium::Services::TrackedAllocator<T>>& GetArray() { return componentArray; }
    std::unordered_map<Entity, size_t, std::hash<Entity>, std::equal_to<Entity>, Elysium::Services::TrackedAllocator<std::pair<const Entity, size_t>>>& GetEntityToIndex() { return entityToIndex; }
};

class ComponentManager {
private:
    std::unordered_map<std::type_index, std::shared_ptr<ComponentArray>, std::hash<std::type_index>, std::equal_to<std::type_index>, Elysium::Services::TrackedAllocator<std::pair<const std::type_index, std::shared_ptr<ComponentArray>>>> componentArrays;
    std::unordered_map<std::type_index, size_t, std::hash<std::type_index>, std::equal_to<std::type_index>, Elysium::Services::TrackedAllocator<std::pair<const std::type_index, size_t>>> componentTypes;
    size_t nextComponentType = 0;
    
    template<typename T>
    std::shared_ptr<TypedComponentArray<T>> GetComponentArray() {
        std::type_index typeName = std::type_index(typeid(T));
        return std::static_pointer_cast<TypedComponentArray<T>>(componentArrays[typeName]);
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
    bool HasComponent(Entity entity) {
        return GetComponentArray<T>()->HasData(entity);
    }
    
    void EntityDestroyed(Entity entity) {
        for (auto const& pair : componentArrays) {
            auto const& component = pair.second;
            component->EntityDestroyed(entity);
        }
    }
    
    template<typename T>
    std::vector<T, Elysium::Services::TrackedAllocator<T>>& GetRawComponentArray() {
        return GetComponentArray<T>()->GetArray();
    }
    
    template<typename T>
    std::unordered_map<Entity, size_t, std::hash<Entity>, std::equal_to<Entity>, Elysium::Services::TrackedAllocator<std::pair<const Entity, size_t>>>& GetEntityToIndexMap() {
        return GetComponentArray<T>()->GetEntityToIndex();
    }
};

class EntityManager {
private:
    std::queue<Entity, std::deque<Entity, Elysium::Services::TrackedAllocator<Entity>>> availableEntities;
    std::vector<ComponentMask, Elysium::Services::TrackedAllocator<ComponentMask>> componentMasks;
    std::vector<Entity, Elysium::Services::TrackedAllocator<Entity>> livingEntities; // Track living entities for fast iteration
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
            return MAX_ENTITIES; // Invalid entity
        }
        
        Entity id = availableEntities.front();
        availableEntities.pop();
        livingEntities.push_back(id);
        ++livingEntityCount;
        return id;
    }
    
    void DestroyEntity(Entity entity) {
        if (entity >= MAX_ENTITIES) return;
        
        componentMasks[entity].reset();
        availableEntities.push(entity);
        
        // Remove from living entities list
        auto it = std::find(livingEntities.begin(), livingEntities.end(), entity);
        if (it != livingEntities.end()) {
            std::swap(*it, livingEntities.back());
            livingEntities.pop_back();
        }
        --livingEntityCount;
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
    
    const std::vector<Entity, Elysium::Services::TrackedAllocator<Entity>>& GetLivingEntities() const {
        return livingEntities;
    }
};

class EntityWorld {
private:
    std::unique_ptr<ComponentManager> componentManager;
    std::unique_ptr<EntityManager> entityManager;
    
    // Pre-allocated vectors for entity queries to avoid frequent allocations
    mutable std::vector<Entity, Elysium::Services::TrackedAllocator<Entity>> queryBuffer1_;
    mutable std::vector<Entity, Elysium::Services::TrackedAllocator<Entity>> queryBuffer2_;
    mutable std::vector<Entity, Elysium::Services::TrackedAllocator<Entity>> queryBuffer3_;
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
    }
    
    Entity CreateEntity() {
        return entityManager->CreateEntity();
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
        return const_cast<ComponentManager*>(componentManager.get())->GetComponent<T>(entity);
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
    const std::vector<Entity, Elysium::Services::TrackedAllocator<Entity>>& GetEntitiesWithComponents() const {
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
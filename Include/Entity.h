#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <variant>
#include <vector>
#include <queue>

struct PositionComponent {
    float x;
    float y;
};

struct SpriteComponent {
    std::string name;
    std::string frame;
};

struct TextComponent {
    std::string content;
    int fontSize;
};

using Component = std::variant<
    PositionComponent,
    SpriteComponent,
    TextComponent
>;

using Entity = size_t;

class EntityCollection {
    std::queue<Entity> free_entities;
    std::unordered_map<std::type_info, std::vector<Component>> components;
public:
    void Initialize(size_t size);

    bool CreateEntity(Entity* entity);
    void DeleteEntity(Entity* entity);

    template <typename T>
    bool HasComponent(const Entity& entity);

    template <typename T>
    bool GetComponent(const Entity& entity, T* component);

    template <typename T>
    bool AddComponent(const Entity& entity, const T& compoent);

    template <typename T>
    bool RemoveComponent(const Entity& entity);
};

struct EntitySystem {
    virtual void OnEvent(const char* eventName, void* data) = 0;
    virtual void OnUpdate(float dt) = 0;
    virtual void OnDraw() = 0;
};
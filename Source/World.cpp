#include "Entity.h"
#include "Sprite.h"

namespace Elysium
{
PositionComponent::PositionComponent(float x, float y) : x(x), y(y)
{
}

LocationComponent::LocationComponent(int x, int y) : x(x), y(y)
{
}

MovementComponent::MovementComponent(const std::vector<Vector2>& waypoints) : waypoints(waypoints) {}

AnimationComponent::AnimationComponent(std::string marker) : marker(marker) {}

LayerComponent::LayerComponent(int z) : zIndex(z), type(Type::World), space(Space::World), blend(Blend::Normal) {}

RectangleComponent::RectangleComponent(float width, float height, Color background, Color border, const std::string& layer)
    : width(width), height(height), background(background), border(border), layerName(layer)
{
}

CircleComponent::CircleComponent(float r, Color background, Color border, const std::string& layer)
    : radius(r), background(background), border(border), layerName(layer)
{
}

SpriteComponent::SpriteComponent(const Sprite& sprite, const std::string& marker, const std::string& layer)
    : sprite(sprite), markerName(marker), layerName(layer)
{
}

TextComponent::TextComponent(const std::string &text, int size, Color c, const std::string& layer) : content(text), fontSize(size), color(c), layerName(layer)
{
}

TeamComponent::TeamComponent() : teamId(0)
{
}
TeamComponent::TeamComponent(int teamId) : teamId(teamId)
{
}

CameraComponent::CameraComponent()
    : position({0.0f, 0.0f}), zoom(1.0f), viewport({0, 0, 800, 600}), renderOrder(0), isVisible(true)
{
}

// ComponentManager implementations
void ComponentManager::EntityDestroyed(Entity entity)
{
    for (auto const &pair : componentArrays)
    {
        auto const &component = pair.second;
        component->EntityDestroyed(entity);
    }
}

// EntityManager implementations
EntityManager::EntityManager()
{
    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        availableEntities.push(entity);
    }
    componentMasks.resize(MAX_ENTITIES);
    livingEntities.reserve(1000);
}

Entity EntityManager::CreateEntity()
{
    if (livingEntityCount >= MAX_ENTITIES)
    {
        return INVALID_ENTITY;
    }
    Entity id = availableEntities.front();
    availableEntities.pop();
    livingEntities.push_back(id);
    ++livingEntityCount;
    return id;
}

Entity EntityManager::CreateEntity(std::string name)
{
    Entity entity = CreateEntity();
    if (entity != INVALID_ENTITY)
    {
        names[entity] = name;
    }
    return entity;
}

void EntityManager::DestroyEntity(Entity entity)
{
    if (entity >= MAX_ENTITIES || livingEntityCount == 0)
    {
        return;
    }
    componentMasks[entity].reset();
    availableEntities.push(entity);
    auto it = std::find(livingEntities.begin(), livingEntities.end(), entity);
    if (it != livingEntities.end())
    {
        std::swap(*it, livingEntities.back());
        livingEntities.pop_back();
        --livingEntityCount;
    }
    auto nameIt = names.find(entity);
    if (nameIt != names.end())
    {
        names.erase(nameIt);
    }
}

bool EntityManager::GetEntityByName(std::string name, Entity *entity)
{
    for (const auto &pair : names)
    {
        if (pair.second == name)
        {
            *entity = pair.first;
            return true;
        }
    }
    return false;
}

void EntityManager::SetComponentMask(Entity entity, ComponentMask mask)
{
    if (entity < MAX_ENTITIES)
    {
        componentMasks[entity] = mask;
    }
}

ComponentMask EntityManager::GetComponentMask(Entity entity)
{
    if (entity < MAX_ENTITIES)
    {
        return componentMasks[entity];
    }
    return ComponentMask();
}

size_t EntityManager::GetLivingEntityCount() const
{
    return livingEntityCount;
}

const TrackedVector<Entity> &EntityManager::GetLivingEntities() const
{
    return livingEntities;
}

// World implementations (already moved from previous fix)
World::World()
{
    componentManager = std::make_unique<ComponentManager>();
    entityManager = std::make_unique<EntityManager>();
    queryBuffer1_.reserve(1000);
    queryBuffer2_.reserve(1000);
    queryBuffer3_.reserve(1000);
    RegisterComponent<LocationComponent>();
    RegisterComponent<PositionComponent>();
    RegisterComponent<MovementComponent>();
    RegisterComponent<DirectionComponent>();
    RegisterComponent<AnimationComponent>();
    RegisterComponent<LayerComponent>();
    RegisterComponent<LightComponent>();
    RegisterComponent<RectangleComponent>();
    RegisterComponent<CircleComponent>();
    RegisterComponent<SpriteComponent>();
    RegisterComponent<TextComponent>();
    RegisterComponent<CameraComponent>();
    RegisterComponent<FollowComponent>();
    RegisterComponent<TileComponent>();
    RegisterComponent<TeamComponent>();
}

Entity World::CreateEntity()
{
    return entityManager->CreateEntity();
}

Entity World::CreateEntity(std::string name)
{
    return entityManager->CreateEntity(name);
}

bool World::GetEntityByName(std::string name, Entity *entity)
{
    return entityManager->GetEntityByName(name, entity);
}

void World::DestroyEntity(Entity entity)
{
    entityManager->DestroyEntity(entity);
    componentManager->EntityDestroyed(entity);
}

size_t World::GetEntityCount() const
{
    return entityManager->GetLivingEntityCount();
}
} // namespace Elysium

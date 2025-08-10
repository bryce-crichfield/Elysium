#include "Entity.h"

namespace Elysium
{
PositionComponent::PositionComponent(float x, float y) : x(x), y(y)
{
}

LocationComponent::LocationComponent(int x, int y) : x(x), y(y)
{
}

VelocityComponent::VelocityComponent(float x, float y) : x(x), y(y)
{
}

PhysicsComponent::PhysicsComponent(float m, float rest, float fric, bool gravity)
    : mass(m), restitution(rest), friction(fric), affectedByGravity(gravity)
{
}

AnimationComponent::AnimationComponent()
{
}

LayerComponent::LayerComponent(int z) : z(z) {}

RectangleComponent::RectangleComponent(float width, float height, Color background, Color border)
    : width(width), height(height), background(background), border(border)
{
}

CircleComponent::CircleComponent(float r, Color background, Color border)
    : radius(r), background(background), border(border)
{
}

SpriteComponent::SpriteComponent(const std::string &name, const std::string &f, Vector2 s, float rot, Color t)
    : textureName(name), frame(f), scale(s), rotation(rot), tint(t)
{
}

TextComponent::TextComponent(const std::string &text, int size, Color c) : content(text), fontSize(size), color(c)
{
}

TeamComponent::TeamComponent() : teamId(0)
{
}
TeamComponent::TeamComponent(int teamId) : teamId(teamId)
{
}

CameraComponent::CameraComponent()
{
}
CameraComponent::CameraComponent(const std::string &target) : target(target)
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
    RegisterComponent<VelocityComponent>();
    RegisterComponent<PhysicsComponent>();
    RegisterComponent<AnimationComponent>();
    RegisterComponent<LayerComponent>();
    RegisterComponent<RectangleComponent>();
    RegisterComponent<CircleComponent>();
    RegisterComponent<SpriteComponent>();
    RegisterComponent<TextComponent>();
    RegisterComponent<CameraComponent>();
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

#include "Entity.h"
#include "Sprite.h"

namespace Elysium
{
void ComponentManager::EntityDestroyed(Entity entity)
{
    for (auto const &pair : componentArrays)
    {
        auto const &component = pair.second;
        component->EntityDestroyed(entity);
    }
}

void ComponentManager::CloneAllComponents(Entity source, Entity dest)
{
    for (auto const &pair : componentArrays)
    {
        auto const &component = pair.second;
        component->CloneComponent(source, dest);
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

const std::vector<Entity> &EntityManager::GetLivingEntities() const
{
    return livingEntities;
}

// World implementations (already moved from previous fix)
World::World()
{
    componentManager = std::make_unique<ComponentManager>();
    entityManager = std::make_unique<EntityManager>();

    RegisterComponent<NameComponent>();
    RegisterComponent<LocationComponent>();
    RegisterComponent<PositionComponent>();
    RegisterComponent<ScaleComponent>();
    RegisterComponent<MovementComponent>();
    RegisterComponent<DirectionComponent>();
    RegisterComponent<AnimationComponent>();
    RegisterComponent<LayerComponent>();
    RegisterComponent<LightComponent>();
    RegisterComponent<RectangleComponent>();
    RegisterComponent<CircleComponent>();
    RegisterComponent<SpriteComponent>();
    RegisterComponent<TextureComponent>();
    RegisterComponent<TextComponent>();
    RegisterComponent<CameraComponent>();
    RegisterComponent<FollowComponent>();
    RegisterComponent<TileComponent>();
    RegisterComponent<TeamComponent>();
    RegisterComponent<CooldownComponent>();
    RegisterComponent<CharacterComponent>();
    RegisterComponent<UnitComponent>();
    RegisterComponent<BoundsComponent>();
}

Entity World::CreateEntity()
{
    return entityManager->CreateEntity();
}

Entity World::CloneEntity(Entity source)
{
    // Create new entity
    Entity newEntity = entityManager->CreateEntity();

    // Copy component mask
    ComponentMask mask = entityManager->GetComponentMask(source);
    entityManager->SetComponentMask(newEntity, mask);

    // Clone all components
    componentManager->CloneAllComponents(source, newEntity);

    return newEntity;
}

bool World::GetEntityByName(std::string name, Entity* entity) {
    bool found = false;
    Query<NameComponent>([&](Entity e, auto &nameComp) {
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

void World::DestroyEntity(Entity entity)
{
    entityManager->DestroyEntity(entity);
    componentManager->EntityDestroyed(entity);
}

size_t World::GetEntityCount() const
{
    return entityManager->GetLivingEntityCount();
}

const std::vector<Entity>& World::GetLivingEntities() const
{
    return entityManager->GetLivingEntities();
}
} // namespace Elysium

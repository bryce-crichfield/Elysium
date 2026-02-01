#pragma once

namespace Elysium {

// World listener - registered with World directly for entity lifecycle events
struct IWorldListener {
    virtual ~IWorldListener() = default;

    virtual void OnEntityCreated(Entity entity) {}
    virtual void OnEntityDestroyed(Entity entity) {}
};

struct EntityCreatedEvent : public Event {
    Entity entity;

    EntityCreatedEvent(Entity e) : entity(e) {}
};

struct EntityDestroyedEvent : public Event {
    Entity entity;

    EntityDestroyedEvent(Entity e) : entity(e) {}
};
}
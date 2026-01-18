#include "../../Include/Network/ComponentSerializer.h"
#include "../../Include/Entity.h"
#include "../../Include/Component.h"

namespace Elysium::Network {

void ComponentSerializer::Initialize() {
    // Register PositionComponent serializer
    RegisterComponent(
        NetworkComponentId::Position,
        // Serialize
        [](Entity entity, World* world, ByteBuffer& buffer) {
            const auto& pos = world->GetComponent<PositionComponent>(entity);
            buffer.WriteFloat(pos.x);
            buffer.WriteFloat(pos.y);
        },
        // Deserialize
        [](Entity entity, World* world, ByteBuffer& buffer) {
            float x = buffer.ReadFloat();
            float y = buffer.ReadFloat();

            if (world->HasComponent<PositionComponent>(entity)) {
                auto& pos = world->GetComponent<PositionComponent>(entity);
                pos.x = x;
                pos.y = y;
            } else {
                world->AddComponent(entity, PositionComponent(x, y));
            }
        },
        // HasComponent
        [](Entity entity, World* world) {
            return world->HasComponent<PositionComponent>(entity);
        }
    );

    // Future: Add more components here
    // RegisterComponent(NetworkComponentId::Scale, ...);
}

void ComponentSerializer::RegisterComponent(
    NetworkComponentId id,
    SerializeFunc serialize,
    DeserializeFunc deserialize,
    HasComponentFunc hasComponent
) {
    handlers_[id] = ComponentHandler{
        std::move(serialize),
        std::move(deserialize),
        std::move(hasComponent)
    };
}

bool ComponentSerializer::SerializeComponent(
    NetworkComponentId id,
    Entity entity,
    World* world,
    ByteBuffer& buffer
) {
    auto it = handlers_.find(id);
    if (it == handlers_.end()) {
        return false;
    }

    if (!it->second.hasComponent(entity, world)) {
        return false;
    }

    it->second.serialize(entity, world, buffer);
    return true;
}

void ComponentSerializer::DeserializeComponent(
    NetworkComponentId id,
    Entity entity,
    World* world,
    ByteBuffer& buffer
) {
    auto it = handlers_.find(id);
    if (it != handlers_.end()) {
        it->second.deserialize(entity, world, buffer);
    }
}

bool ComponentSerializer::HasNetworkComponent(
    NetworkComponentId id,
    Entity entity,
    World* world
) {
    auto it = handlers_.find(id);
    if (it == handlers_.end()) {
        return false;
    }
    return it->second.hasComponent(entity, world);
}

uint32_t ComponentSerializer::SerializeAllComponents(
    Entity entity,
    World* world,
    ByteBuffer& buffer
) {
    uint32_t mask = 0;

    for (const auto& [id, handler] : handlers_) {
        if (handler.hasComponent(entity, world)) {
            handler.serialize(entity, world, buffer);
            mask |= GetComponentMaskBit(id);
        }
    }

    return mask;
}

void ComponentSerializer::DeserializeComponents(
    uint32_t componentMask,
    Entity entity,
    World* world,
    ByteBuffer& buffer
) {
    // Iterate through handlers in ID order to ensure consistent deserialization
    for (uint8_t i = 0; i <= static_cast<uint8_t>(NetworkComponentId::MAX_COMPONENT); ++i) {
        if (componentMask & (1u << i)) {
            auto id = static_cast<NetworkComponentId>(i);
            auto it = handlers_.find(id);
            if (it != handlers_.end()) {
                it->second.deserialize(entity, world, buffer);
            }
        }
    }
}

uint32_t ComponentSerializer::GetNetworkComponentMask(Entity entity, World* world) {
    uint32_t mask = 0;

    for (const auto& [id, handler] : handlers_) {
        if (handler.hasComponent(entity, world)) {
            mask |= GetComponentMaskBit(id);
        }
    }

    return mask;
}

} // namespace Elysium::Network

#include "../../Include/Network/ComponentSerializer.h"
#include "../../Include/Component.h"
#include "../../Include/Entity.h"

namespace Elysium::Network {

// =============================================================================
// Network Serializers Registry
// Pattern mirrors ComponentSavers() from SceneSaver.cpp
// =============================================================================
const std::unordered_map<NetworkComponentId, NetworkSerializer>& NetworkSerializers() {
    static std::unordered_map<NetworkComponentId, NetworkSerializer> serializers;

    if (!serializers.empty())
        return serializers;

    // PositionComponent
    serializers[NetworkComponentId::Position] = [](Entity entity, World* world, ByteBuffer& buffer) {
        const auto& pos = world->GetComponent<PositionComponent>(entity);
        buffer.WriteFloat(pos.x);
        buffer.WriteFloat(pos.y);
    };

    // RectangleComponent
    serializers[NetworkComponentId::Rectangle] = [](Entity entity, World* world, ByteBuffer& buffer) {
        const auto& rect = world->GetComponent<RectangleComponent>(entity);
        buffer.WriteFloat(rect.width);
        buffer.WriteFloat(rect.height);
        buffer.WriteU8(rect.background.r);
        buffer.WriteU8(rect.background.g);
        buffer.WriteU8(rect.background.b);
        buffer.WriteU8(rect.background.a);
        buffer.WriteU8(rect.border.r);
        buffer.WriteU8(rect.border.g);
        buffer.WriteU8(rect.border.b);
        buffer.WriteU8(rect.border.a);
        // Note: layerName is not synced over network (local rendering concern)
    };

    return serializers;
}

// =============================================================================
// Network Deserializers Registry
// Pattern mirrors ComponentLoaders() from SceneLoader.cpp
// =============================================================================
const std::unordered_map<NetworkComponentId, NetworkDeserializer>& NetworkDeserializers() {
    static std::unordered_map<NetworkComponentId, NetworkDeserializer> deserializers;

    if (!deserializers.empty())
        return deserializers;

    // PositionComponent
    deserializers[NetworkComponentId::Position] = [](Entity entity, World* world, ByteBuffer& buffer) {
        float x = buffer.ReadFloat();
        float y = buffer.ReadFloat();

        if (world->HasComponent<PositionComponent>(entity)) {
            auto& pos = world->GetComponent<PositionComponent>(entity);
            pos.x = x;
            pos.y = y;
        } else {
            world->AddComponent(entity, PositionComponent(x, y));
        }
    };

    // RectangleComponent
    deserializers[NetworkComponentId::Rectangle] = [](Entity entity, World* world, ByteBuffer& buffer) {
        float width = buffer.ReadFloat();
        float height = buffer.ReadFloat();
        Color background;
        background.r = buffer.ReadU8();
        background.g = buffer.ReadU8();
        background.b = buffer.ReadU8();
        background.a = buffer.ReadU8();
        Color border;
        border.r = buffer.ReadU8();
        border.g = buffer.ReadU8();
        border.b = buffer.ReadU8();
        border.a = buffer.ReadU8();

        if (world->HasComponent<RectangleComponent>(entity)) {
            auto& rect = world->GetComponent<RectangleComponent>(entity);
            rect.width = width;
            rect.height = height;
            rect.background = background;
            rect.border = border;
        } else {
            world->AddComponent(entity, RectangleComponent(width, height, background, border));
        }
    };

    return deserializers;
}

// =============================================================================
// Network HasComponent Registry
// =============================================================================
const std::unordered_map<NetworkComponentId, NetworkHasComponent>& NetworkHasComponents() {
    static std::unordered_map<NetworkComponentId, NetworkHasComponent> hasComponents;

    if (!hasComponents.empty())
        return hasComponents;

    hasComponents[NetworkComponentId::Position] = [](Entity entity, World* world) {
        return world->HasComponent<PositionComponent>(entity);
    };

    hasComponents[NetworkComponentId::Rectangle] = [](Entity entity, World* world) {
        return world->HasComponent<RectangleComponent>(entity);
    };

    return hasComponents;
}

// =============================================================================
// Helper Functions
// =============================================================================

bool HasNetworkComponent(NetworkComponentId id, Entity entity, World* world) {
    const auto& checks = NetworkHasComponents();
    auto it = checks.find(id);
    if (it == checks.end()) {
        return false;
    }
    return it->second(entity, world);
}

bool SerializeComponent(NetworkComponentId id, Entity entity, World* world, ByteBuffer& buffer) {
    if (!HasNetworkComponent(id, entity, world)) {
        return false;
    }

    const auto& serializers = NetworkSerializers();
    auto it = serializers.find(id);
    if (it == serializers.end()) {
        return false;
    }

    it->second(entity, world, buffer);
    return true;
}

void DeserializeComponent(NetworkComponentId id, Entity entity, World* world, ByteBuffer& buffer) {
    const auto& deserializers = NetworkDeserializers();
    auto it = deserializers.find(id);
    if (it != deserializers.end()) {
        it->second(entity, world, buffer);
    }
}

uint32_t GetNetworkComponentMask(Entity entity, World* world) {
    uint32_t mask = 0;

    const auto& checks = NetworkHasComponents();
    for (const auto& [id, hasComponent] : checks) {
        if (hasComponent(entity, world)) {
            mask |= GetComponentMaskBit(id);
        }
    }

    return mask;
}

void SerializeComponentsByMask(uint32_t componentMask, Entity entity, World* world, ByteBuffer& buffer) {
    const auto& serializers = NetworkSerializers();

    // Iterate in ID order for consistent serialization
    for (uint8_t i = 0; i <= static_cast<uint8_t>(NetworkComponentId::MAX_COMPONENT); ++i) {
        if (componentMask & (1u << i)) {
            auto id = static_cast<NetworkComponentId>(i);
            auto it = serializers.find(id);
            if (it != serializers.end()) {
                it->second(entity, world, buffer);
            }
        }
    }
}

void DeserializeComponentsByMask(uint32_t componentMask, Entity entity, World* world, ByteBuffer& buffer) {
    const auto& deserializers = NetworkDeserializers();

    // Iterate in ID order for consistent deserialization
    for (uint8_t i = 0; i <= static_cast<uint8_t>(NetworkComponentId::MAX_COMPONENT); ++i) {
        if (componentMask & (1u << i)) {
            auto id = static_cast<NetworkComponentId>(i);
            auto it = deserializers.find(id);
            if (it != deserializers.end()) {
                it->second(entity, world, buffer);
            }
        }
    }
}

}  // namespace Elysium::Network
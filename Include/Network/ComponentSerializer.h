#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include "ByteBuffer.h"
#include "../Entity.h"

namespace Elysium::Network {

/**
 * Network Component IDs
 * Stable across builds - used for binary serialization.
 * Do not reorder or remove IDs - only append new ones.
 */
enum class NetworkComponentId : uint8_t {
    Position = 0,
    // Future: Scale = 1, Location = 2, etc.

    MAX_COMPONENT = 31  // Maximum 32 components (fits in uint32_t bitmask)
};

/**
 * Get component mask bit for a network component ID
 */
inline uint32_t GetComponentMaskBit(NetworkComponentId id) {
    return 1u << static_cast<uint8_t>(id);
}

/**
 * ComponentSerializer - Registry-based binary serialization for networked components
 *
 * Pattern mirrors the existing XML ComponentLoader/Saver architecture.
 * Each component type has registered serialize/deserialize functions.
 *
 * Usage:
 *   ComponentSerializer serializer;
 *   serializer.Initialize();  // Registers default component handlers
 *
 *   // Serialize an entity's Position
 *   serializer.SerializeComponent(NetworkComponentId::Position, entity, world, buffer);
 *
 *   // Deserialize received Position
 *   serializer.DeserializeComponent(NetworkComponentId::Position, entity, world, buffer);
 */
class ComponentSerializer {
public:
    using SerializeFunc = std::function<void(Entity, World*, ByteBuffer&)>;
    using DeserializeFunc = std::function<void(Entity, World*, ByteBuffer&)>;
    using HasComponentFunc = std::function<bool(Entity, World*)>;

    ComponentSerializer() = default;

    /**
     * Initialize with default component serializers (Position, etc.)
     */
    void Initialize();

    /**
     * Register a custom component serializer
     */
    void RegisterComponent(
        NetworkComponentId id,
        SerializeFunc serialize,
        DeserializeFunc deserialize,
        HasComponentFunc hasComponent
    );

    /**
     * Serialize a specific component to the buffer
     * Returns true if component exists and was serialized
     */
    bool SerializeComponent(NetworkComponentId id, Entity entity, World* world, ByteBuffer& buffer);

    /**
     * Deserialize a component from the buffer
     */
    void DeserializeComponent(NetworkComponentId id, Entity entity, World* world, ByteBuffer& buffer);

    /**
     * Check if entity has a networked component
     */
    bool HasNetworkComponent(NetworkComponentId id, Entity entity, World* world);

    /**
     * Serialize all networked components for an entity
     * Returns the component mask of serialized components
     */
    uint32_t SerializeAllComponents(Entity entity, World* world, ByteBuffer& buffer);

    /**
     * Deserialize components based on a component mask
     */
    void DeserializeComponents(uint32_t componentMask, Entity entity, World* world, ByteBuffer& buffer);

    /**
     * Get the component mask for all networked components an entity has
     */
    uint32_t GetNetworkComponentMask(Entity entity, World* world);

private:
    struct ComponentHandler {
        SerializeFunc serialize;
        DeserializeFunc deserialize;
        HasComponentFunc hasComponent;
    };

    std::unordered_map<NetworkComponentId, ComponentHandler> handlers_;
};

} // namespace Elysium::Network

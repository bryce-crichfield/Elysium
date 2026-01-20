#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include "../Entity.h"
#include "ByteBuffer.h"

namespace Elysium::Network {

/**
 * Network Component IDs
 * Stable across builds - used for binary serialization.
 * Do not reorder or remove IDs - only append new ones.
 */
enum class NetworkComponentId : uint8_t {
    Position = 0,
    Rectangle = 1,
    // Future: Scale = 2, Velocity = 3, etc.

    MAX_COMPONENT = 31  // Maximum 32 components (fits in uint32_t bitmask)
};

/**
 * Get component mask bit for a network component ID
 */
inline uint32_t GetComponentMaskBit(NetworkComponentId id) {
    return 1u << static_cast<uint8_t>(id);
}

/**
 * Function signatures for network serialization
 * Mirrors the XML ComponentLoader/ComponentSaver pattern
 */
using NetworkSerializer = std::function<void(Entity, World*, ByteBuffer&)>;
using NetworkDeserializer = std::function<void(Entity, World*, ByteBuffer&)>;
using NetworkHasComponent = std::function<bool(Entity, World*)>;

/**
 * Get the registry of network serializers
 * Pattern mirrors ComponentSavers() from SceneSaver.cpp
 */
const std::unordered_map<NetworkComponentId, NetworkSerializer>& NetworkSerializers();

/**
 * Get the registry of network deserializers
 * Pattern mirrors ComponentLoaders() from SceneLoader.cpp
 */
const std::unordered_map<NetworkComponentId, NetworkDeserializer>& NetworkDeserializers();

/**
 * Get the registry of component existence checks
 */
const std::unordered_map<NetworkComponentId, NetworkHasComponent>& NetworkHasComponents();

/**
 * Check if entity has a networked component by ID
 */
bool HasNetworkComponent(NetworkComponentId id, Entity entity, World* world);

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
 * Get the component mask for all networked components an entity has
 */
uint32_t GetNetworkComponentMask(Entity entity, World* world);

/**
 * Serialize all components indicated by the mask
 */
void SerializeComponentsByMask(uint32_t componentMask, Entity entity, World* world, ByteBuffer& buffer);

/**
 * Deserialize components based on a component mask
 */
void DeserializeComponentsByMask(uint32_t componentMask, Entity entity, World* world, ByteBuffer& buffer);

}  // namespace Elysium::Network
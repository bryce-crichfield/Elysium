#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include "ByteBuffer.h"

namespace Elysium::Network {

/**
 * Network Protocol Constants
 */
constexpr uint32_t PROTOCOL_VERSION = 1;
constexpr uint32_t SERVER_TICK_RATE_MS = 50;  // 20 Hz
constexpr uint32_t MAX_INPUTS_PER_PACKET = 32;
constexpr uint32_t MAX_ENTITIES_PER_SYNC = 128;

/**
 * Packet Types - Direction indicates primary flow
 */
enum class PacketType : uint8_t {
    // Connection Management
    HandshakeRequest = 0x01,   // C -> S: Client requests connection
    HandshakeResponse = 0x02,  // S -> C: Server accepts, sends full state

    // Input Forwarding
    InputPacket = 0x10,  // C -> S: Client inputs (batched)

    // State Synchronization
    SyncPacket = 0x20,       // S -> C: Dirty component updates
    EntityCreated = 0x21,    // S -> C: New entity notification
    EntityDestroyed = 0x22,  // S -> C: Entity removal notification

    // Scene Control
    SceneChange = 0x23,  // S -> C: Server tells client to change scene

    // Utility
    Ping = 0x30,  // Bidirectional: Latency measurement
    Pong = 0x31,  // Response to Ping
};

/**
 * Packet Header - Prepended to all packets
 * Total size: 8 bytes
 */
struct PacketHeader {
    uint8_t packetType;    // PacketType enum
    uint8_t flags;         // Reserved for future use
    uint16_t payloadSize;  // Size of payload after header
    uint32_t tick;         // Server tick number

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU8(packetType);
        buffer.WriteU8(flags);
        buffer.WriteU16(payloadSize);
        buffer.WriteU32(tick);
    }

    void Read(ByteBuffer& buffer) {
        packetType = buffer.ReadU8();
        flags = buffer.ReadU8();
        payloadSize = buffer.ReadU16();
        tick = buffer.ReadU32();
    }

    static constexpr size_t SIZE = 8;
};

/**
 * Handshake Request Packet
 * Client -> Server
 */
struct HandshakeRequestPacket {
    uint32_t protocolVersion;
    // Future: player name, auth token, etc.

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU32(protocolVersion);
    }

    void Read(ByteBuffer& buffer) {
        protocolVersion = buffer.ReadU32();
    }
};

/**
 * Handshake Response Packet
 * Server -> Client
 * Contains full world state for initial sync
 */
struct HandshakeResponsePacket {
    uint32_t protocolVersion;
    uint32_t serverTick;
    uint32_t entityCount;
    // Followed by: entity data (serialized by ComponentSerializer)

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU32(protocolVersion);
        buffer.WriteU32(serverTick);
        buffer.WriteU32(entityCount);
    }

    void Read(ByteBuffer& buffer) {
        protocolVersion = buffer.ReadU32();
        serverTick = buffer.ReadU32();
        entityCount = buffer.ReadU32();
    }
};

/**
 * Sync Packet Header
 * Server -> Client (every tick with dirty entities)
 * Server sends [PacketHeader, SyncPacketHeader, EntitySyncHeader, ComponentData ..., EntitySyncHeader, ComponentData ...]
 */
struct SyncPacketHeader {
    uint32_t serverTick;
    uint32_t lastProcessedInput;  // For client-side prediction reconciliation
    uint16_t entityCount;         // Number of entities in this packet

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU32(serverTick);
        buffer.WriteU32(lastProcessedInput);
        buffer.WriteU16(entityCount);
    }

    void Read(ByteBuffer& buffer) {
        serverTick = buffer.ReadU32();
        lastProcessedInput = buffer.ReadU32();
        entityCount = buffer.ReadU16();
    }

    static constexpr size_t SIZE = 10;
};

/**
 * Entity Sync Entry Header
 * Describes which entity and what components follow
 */
struct EntitySyncHeader {
    uint32_t entityId;       // Entity ID (size_t cast to u32)
    uint32_t componentMask;  // Bitmask of which components follow

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU32(entityId);
        buffer.WriteU32(componentMask);
    }

    void Read(ByteBuffer& buffer) {
        entityId = buffer.ReadU32();
        componentMask = buffer.ReadU32();
    }

    static constexpr size_t SIZE = 8;
};

/**
 * Entity Created Packet
 * Server -> Client: Notifies of new entity creation
 */
struct EntityCreatedPacket {
    uint32_t entityId;
    uint32_t componentMask;  // Initial components
    // Followed by: component data

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU32(entityId);
        buffer.WriteU32(componentMask);
    }

    void Read(ByteBuffer& buffer) {
        entityId = buffer.ReadU32();
        componentMask = buffer.ReadU32();
    }
};

/**
 * Entity Destroyed Packet
 * Server -> Client: Notifies of entity destruction
 */
struct EntityDestroyedPacket {
    uint32_t entityId;

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU32(entityId);
    }

    void Read(ByteBuffer& buffer) {
        entityId = buffer.ReadU32();
    }
};

/**
 * Scene Change Operation
 */
enum class SceneChangeOp : uint8_t {
    Push = 0,     // Push scene onto stack
    Pop = 1,      // Pop current scene
    Replace = 2,  // Replace current scene
    Clear = 3,    // Clear stack and push
};

/**
 * Scene Change Packet
 * Server -> Client: Tells client to change scene
 * Scene name is a fixed-length string (max 64 chars)
 */
struct SceneChangePacket {
    static constexpr size_t MAX_SCENE_NAME_LENGTH = 64;

    SceneChangeOp operation;
    uint8_t sceneNameLength;
    char sceneName[MAX_SCENE_NAME_LENGTH];

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU8(static_cast<uint8_t>(operation));
        buffer.WriteU8(sceneNameLength);
        buffer.WriteBytes(reinterpret_cast<const uint8_t*>(sceneName), sceneNameLength);
    }

    void Read(ByteBuffer& buffer) {
        operation = static_cast<SceneChangeOp>(buffer.ReadU8());
        sceneNameLength = buffer.ReadU8();
        if (sceneNameLength > MAX_SCENE_NAME_LENGTH) {
            sceneNameLength = MAX_SCENE_NAME_LENGTH;
        }
        std::memset(sceneName, 0, MAX_SCENE_NAME_LENGTH);
        buffer.ReadBytes(reinterpret_cast<uint8_t*>(sceneName), sceneNameLength);
    }

    std::string GetSceneName() const {
        return std::string(sceneName, sceneNameLength);
    }

    void SetSceneName(const std::string& name) {
        sceneNameLength = static_cast<uint8_t>(
            std::min(name.size(), MAX_SCENE_NAME_LENGTH - 1));
        std::memcpy(sceneName, name.c_str(), sceneNameLength);
        sceneName[sceneNameLength] = '\0';
    }
};

/**
 * Ping/Pong Packets - For latency measurement
 */
struct PingPacket {
    uint32_t timestamp;  // Sender's timestamp

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU32(timestamp);
    }

    void Read(ByteBuffer& buffer) {
        timestamp = buffer.ReadU32();
    }
};

/**
 * Network Channel IDs
 * ENet supports multiple channels for prioritization
 */
enum class NetworkChannel : uint8_t {
    Reliable = 0,    // Important state sync, entity lifecycle
    Unreliable = 1,  // Position updates, inputs (can be dropped)
};

}  // namespace Elysium::Network

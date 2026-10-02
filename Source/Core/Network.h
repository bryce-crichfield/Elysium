#pragma once

#include <cstdint>
#include <string>
#include "../Core/Serial.h"
#include "../Core/Message.h"

namespace Elysium {

using NetworkPeer = size_t;
using InvokeMethodId = uint16_t;
inline constexpr NetworkPeer INVALID_PEER = -1;
inline constexpr NetworkPeer LOCAL_PEER = 0;
inline constexpr NetworkPeer SERVER_PEER = 1;

enum class NetworkMode {
    None,
    Server,
    Client
};

class NetworkDataMessage : public Message {
public:
    NetworkDataMessage(NetworkPeer peer, std::vector<uint8_t> data, uint8_t channel)
        : peer(peer), data(std::move(data)), channel(channel) {}

    NetworkPeer peer;
    uint8_t channel;
    std::vector<uint8_t> data;  // owned, not a raw pointer
};

class NetworkConnectedMessage : public Message {
public:
    NetworkConnectedMessage(NetworkMode mode, void* peer = nullptr)
        : mode(mode), peer(peer) {}
    NetworkMode mode;
    void* peer;
};

class NetworkDisconnectedMessage : public Message {
public:
    NetworkDisconnectedMessage(NetworkMode mode, void* peer = nullptr)
        : mode(mode), peer(peer) {}
    NetworkMode mode;
    void* peer;
};

class NetworkStoppedMessage : public Message {
public:
    NetworkStoppedMessage() = default;
};

struct NetworkConfig {
    NetworkMode mode = NetworkMode::None;
    std::string address = "127.0.0.1";
    uint16_t port = 7777;
    size_t maxClients = 32;
    size_t channelCount = 2;
};

struct NetworkStatistics {
    uint32_t connectedPeers = 0;
    uint32_t packetsSent = 0;
    uint32_t packetsReceived = 0;
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
};

constexpr uint32_t PROTOCOL_VERSION = 1;
constexpr uint32_t SERVER_TICK_RATE_MS = 50;  // 20 Hz
constexpr uint32_t MAX_ENTITIES_PER_SYNC = 128;

enum class NetworkChannel : uint8_t {
    Reliable = 0,    // Connection management, RPCs, create/destroy entities
    Unreliable = 1,  // Update packets
};

enum class PacketType : uint8_t {
    /* --- Connection Lifecycle (Reliable) --- */
    HandshakeRequest  = 0x01, // C -> S: "I want to join"
    HandshakeResponse = 0x02, // S -> C: "Welcome, here is your PlayerID and World Settings"

    /* --- Entity Lifecycle (Reliable) --- */
    EntitySpawn       = 0x10, // S -> C: Create new entity + initial data
    EntityDespawn     = 0x11, // S -> C: Destroy entity ID

    /* --- Entity Synchronization (Unreliable) --- */
    EntityUpdated      = 0x20, // S -> C: Continuous component delta updates

    /* --- Application Layer / RPC (Reliable) --- */
    InvokeRequest     = 0x40, // Bidi: Execute arbitrary code
    InvokeResponse    = 0x41  // Bidi: Result of that execution
};

struct PacketHeader {
    uint8_t packetType = 0;
    uint8_t flags = 0;
    uint16_t payloadSize = 0;
    uint32_t tick = 0;

    static void Write(const PacketHeader& self, SerialBuffer& buffer);
    static void Read(PacketHeader& self, SerialBuffer& buffer);

    static constexpr size_t SIZE = 8;
};

struct HandshakeRequestPacket {
    uint32_t protocolVersion = 0;

    static void Write(const HandshakeRequestPacket& self, SerialBuffer& buffer);
    static void Read(HandshakeRequestPacket& self, SerialBuffer& buffer);
};

struct HandshakeResponsePacket {
    uint32_t protocolVersion = 0;
    uint32_t serverTick = 0;
    uint32_t assignedPeerId = 0;

    static void Write(const HandshakeResponsePacket& self, SerialBuffer& buffer);
    static void Read(HandshakeResponsePacket& self, SerialBuffer& buffer);
};

struct EntityUpdatedPacketHeader {
    uint32_t sceneId = 0;
    uint32_t entityId = 0;
    uint32_t componentMask = 0;

    static void Write(const EntityUpdatedPacketHeader& self, SerialBuffer& buffer);
    static void Read(EntityUpdatedPacketHeader& self, SerialBuffer& buffer);
};

struct EntitySpawnedPacketHeader {
    uint32_t sceneId = 0;
    uint32_t entityId = 0;
    uint32_t componentMask = 0;

    static void Write(const EntitySpawnedPacketHeader& self, SerialBuffer& buffer);
    static void Read(EntitySpawnedPacketHeader& self, SerialBuffer& buffer);
};

struct EntityDespawnedPacket {
    uint32_t sceneId = 0;
    uint32_t entityId = 0;

    static void Write(const EntityDespawnedPacket& self, SerialBuffer& buffer);
    static void Read(EntityDespawnedPacket& self, SerialBuffer& buffer);
};

struct InvokeHeader {
    uint32_t invokeId = 0;
    uint16_t procedureId = 0;
    uint16_t payloadSize = 0;

    static void Write(const InvokeHeader& self, SerialBuffer& buffer);
    static void Read(InvokeHeader& self, SerialBuffer& buffer);

    static constexpr size_t SIZE = 8;
};

class PacketWriter {
public:
    PacketWriter() = default;

    PacketWriter& BeginPacket(PacketType type, uint32_t tick = 0);
    PacketWriter& WriteHeader(uint8_t flags = 0);

    template <Serializable T>
    PacketWriter& Write(const T& data) {
        EnsureHeader();
        T::Write(data, buffer_);
        return *this;
    }

    PacketWriter& WriteInvokeHeader(uint32_t invokeId, uint16_t procedureId);
    PacketWriter& WriteEntityUpdatedHeader(uint32_t sceneId, uint32_t entityId, uint32_t componentMask);
    PacketWriter& WriteEntitySpawnedHeader(uint32_t sceneId, uint32_t entityId, uint32_t componentMask);

    PacketWriter& WriteU8(uint8_t value);
    PacketWriter& WriteU16(uint16_t value);
    PacketWriter& WriteU32(uint32_t value);
    PacketWriter& WriteFloat(float value);
    PacketWriter& WriteBytes(const void* data, size_t size);

    SerialBuffer Build();
    const SerialBuffer& GetBuffer() const;
    void Reset();

private:
    void EnsureHeader();

    SerialBuffer buffer_;
    bool hasHeader_ = false;
    size_t headerPosition_ = 0;
    size_t payloadStart_ = 0;
    uint8_t currentPacketType_ = 0;
    uint32_t currentTick_ = 0;
};

class PacketReader {
public:
    explicit PacketReader(SerialBuffer& buffer);

    template <Serializable T>
    T Read() {
        T data{};
        T::Read(data, buffer_);
        return data;
    }

    uint8_t ReadU8();
    uint16_t ReadU16();
    uint32_t ReadU32();
    float ReadFloat();

    SerialBuffer& GetBuffer();
    const SerialBuffer& GetBuffer() const;
    size_t RemainingBytes() const;

private:
    SerialBuffer& buffer_;
};

}  // namespace Elysium

namespace Elysium::Services {
template <typename T>
struct InvokeMethod;
}  // namespace Elysium::Services

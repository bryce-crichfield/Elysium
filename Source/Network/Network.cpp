#include "Network.h"

namespace Elysium {

// --- PacketHeader ---

void PacketHeader::Write(const PacketHeader& self, SerialBuffer& buffer) {
    buffer.WriteU8(self.packetType);
    buffer.WriteU8(self.flags);
    buffer.WriteU16(self.payloadSize);
    buffer.WriteU32(self.tick);
}

void PacketHeader::Read(PacketHeader& self, SerialBuffer& buffer) {
    self.packetType = buffer.ReadU8();
    self.flags = buffer.ReadU8();
    self.payloadSize = buffer.ReadU16();
    self.tick = buffer.ReadU32();
}

// --- HandshakeRequestPacket ---

void HandshakeRequestPacket::Write(const HandshakeRequestPacket& self, SerialBuffer& buffer) {
    buffer.WriteU32(self.protocolVersion);
}

void HandshakeRequestPacket::Read(HandshakeRequestPacket& self, SerialBuffer& buffer) {
    self.protocolVersion = buffer.ReadU32();
}

// --- HandshakeResponsePacket ---

void HandshakeResponsePacket::Write(const HandshakeResponsePacket& self, SerialBuffer& buffer) {
    buffer.WriteU32(self.protocolVersion);
    buffer.WriteU32(self.serverTick);
    buffer.WriteU32(self.assignedPeerId);
}

void HandshakeResponsePacket::Read(HandshakeResponsePacket& self, SerialBuffer& buffer) {
    self.protocolVersion = buffer.ReadU32();
    self.serverTick = buffer.ReadU32();
    self.assignedPeerId = buffer.ReadU32();
}

// --- EntityUpdatedPacketHeader ---

void EntityUpdatedPacketHeader::Write(const EntityUpdatedPacketHeader& self, SerialBuffer& buffer) {
    buffer.WriteU32(self.sceneId);
    buffer.WriteU32(self.entityId);
    buffer.WriteU32(self.componentMask);
}

void EntityUpdatedPacketHeader::Read(EntityUpdatedPacketHeader& self, SerialBuffer& buffer) {
    self.sceneId = buffer.ReadU32();
    self.entityId = buffer.ReadU32();
    self.componentMask = buffer.ReadU32();
}

// --- EntitySpawnedPacketHeader ---

void EntitySpawnedPacketHeader::Write(const EntitySpawnedPacketHeader& self, SerialBuffer& buffer) {
    buffer.WriteU32(self.sceneId);
    buffer.WriteU32(self.entityId);
    buffer.WriteU32(self.componentMask);
}

void EntitySpawnedPacketHeader::Read(EntitySpawnedPacketHeader& self, SerialBuffer& buffer) {
    self.sceneId = buffer.ReadU32();
    self.entityId = buffer.ReadU32();
    self.componentMask = buffer.ReadU32();
}

// --- EntityDespawnedPacket ---

void EntityDespawnedPacket::Write(const EntityDespawnedPacket& self, SerialBuffer& buffer) {
    buffer.WriteU32(self.sceneId);
    buffer.WriteU32(self.entityId);
}

void EntityDespawnedPacket::Read(EntityDespawnedPacket& self, SerialBuffer& buffer) {
    self.sceneId = buffer.ReadU32();
    self.entityId = buffer.ReadU32();
}

// --- InvokeHeader ---

void InvokeHeader::Write(const InvokeHeader& self, SerialBuffer& buffer) {
    buffer.WriteU32(self.invokeId);
    buffer.WriteU16(self.procedureId);
    buffer.WriteU16(self.payloadSize);
}

void InvokeHeader::Read(InvokeHeader& self, SerialBuffer& buffer) {
    self.invokeId = buffer.ReadU32();
    self.procedureId = buffer.ReadU16();
    self.payloadSize = buffer.ReadU16();
}

// --- PacketWriter ---

PacketWriter& PacketWriter::BeginPacket(PacketType type, uint32_t tick) {
    buffer_.Clear();
    currentPacketType_ = static_cast<uint8_t>(type);
    currentTick_ = tick;
    hasHeader_ = false;
    return *this;
}

PacketWriter& PacketWriter::WriteHeader(uint8_t flags) {
    if (hasHeader_) {
        throw std::runtime_error("Header already written");
    }

    headerPosition_ = buffer_.Size();

    PacketHeader header;
    header.packetType = currentPacketType_;
    header.flags = flags;
    header.payloadSize = 0;  // Filled in Build()
    header.tick = currentTick_;
    PacketHeader::Write(header, buffer_);

    payloadStart_ = buffer_.Size();
    hasHeader_ = true;
    return *this;
}

PacketWriter& PacketWriter::WriteInvokeHeader(uint32_t invokeId, uint16_t procedureId) {
    EnsureHeader();

    InvokeHeader header;
    header.invokeId = invokeId;
    header.procedureId = procedureId;
    header.payloadSize = 0;
    InvokeHeader::Write(header, buffer_);

    return *this;
}

PacketWriter& PacketWriter::WriteEntityUpdatedHeader(uint32_t sceneId, uint32_t entityId, uint32_t componentMask) {
    EnsureHeader();

    EntityUpdatedPacketHeader header;
    header.sceneId = sceneId;
    header.entityId = entityId;
    header.componentMask = componentMask;
    EntityUpdatedPacketHeader::Write(header, buffer_);

    return *this;
}

PacketWriter& PacketWriter::WriteEntitySpawnedHeader(uint32_t sceneId, uint32_t entityId, uint32_t componentMask) {
    EnsureHeader();

    EntitySpawnedPacketHeader header;
    header.sceneId = sceneId;
    header.entityId = entityId;
    header.componentMask = componentMask;
    EntitySpawnedPacketHeader::Write(header, buffer_);

    return *this;
}

PacketWriter& PacketWriter::WriteU8(uint8_t value) {
    EnsureHeader();
    buffer_.WriteU8(value);
    return *this;
}

PacketWriter& PacketWriter::WriteU16(uint16_t value) {
    EnsureHeader();
    buffer_.WriteU16(value);
    return *this;
}

PacketWriter& PacketWriter::WriteU32(uint32_t value) {
    EnsureHeader();
    buffer_.WriteU32(value);
    return *this;
}

PacketWriter& PacketWriter::WriteFloat(float value) {
    EnsureHeader();
    buffer_.WriteFloat(value);
    return *this;
}

PacketWriter& PacketWriter::WriteBytes(const void* data, size_t size) {
    EnsureHeader();
    buffer_.WriteBytes(data, size);
    return *this;
}

SerialBuffer PacketWriter::Build() {
    if (!hasHeader_) {
        WriteHeader();
    }

    // Calculate payload size and patch it into the header
    uint16_t payloadSize = static_cast<uint16_t>(buffer_.Size() - payloadStart_);

    // payloadSize sits at headerPosition_ + 2 (after packetType and flags)
    auto& raw = buffer_.GetBuffer();
    size_t offset = headerPosition_ + 2;
    raw[offset]     = static_cast<uint8_t>(payloadSize & 0xFF);
    raw[offset + 1] = static_cast<uint8_t>((payloadSize >> 8) & 0xFF);

    return buffer_;
}

const SerialBuffer& PacketWriter::GetBuffer() const {
    return buffer_;
}

void PacketWriter::Reset() {
    buffer_.Clear();
    hasHeader_ = false;
    headerPosition_ = 0;
    payloadStart_ = 0;
    currentPacketType_ = 0;
    currentTick_ = 0;
}

void PacketWriter::EnsureHeader() {
    if (!hasHeader_) {
        WriteHeader();
    }
}

// --- PacketReader ---

PacketReader::PacketReader(SerialBuffer& buffer) : buffer_(buffer) {}

uint8_t PacketReader::ReadU8() { return buffer_.ReadU8(); }
uint16_t PacketReader::ReadU16() { return buffer_.ReadU16(); }
uint32_t PacketReader::ReadU32() { return buffer_.ReadU32(); }
float PacketReader::ReadFloat() { return buffer_.ReadFloat(); }

SerialBuffer& PacketReader::GetBuffer() { return buffer_; }
const SerialBuffer& PacketReader::GetBuffer() const { return buffer_; }

size_t PacketReader::RemainingBytes() const {
    return buffer_.Size() - buffer_.ReadPosition();
}

}  // namespace Elysium

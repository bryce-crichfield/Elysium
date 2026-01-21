#include "ClientNetworkSystem.h"
#include <tracy/Tracy.hpp>
#include "Core/Application.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Messages/NetworkMessages.h"
#include "Network/ByteBuffer.h"
#include "Core/Scene.h"
#include "Services/NetworkService.h"
#include "raylib.h"

namespace Elysium {

ClientNetworkSystem::ClientNetworkSystem(Context context)
    : System(context) {
    // No initialization needed - using free functions now
}

void ClientNetworkSystem::Update(float deltaTime) {
    ZoneScopedN("ClientNetworkSystem::Update");

    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();

    // Only process if we're running as client
    if (networkService.GetMode() != Services::NetworkMode::Client) {
        return;
    }

    float deltaMs = deltaTime * 1000.0f;

    // Flush inputs periodically
    inputFlushTimer_ += deltaMs;
    if (inputFlushTimer_ >= INPUT_FLUSH_INTERVAL_MS) {
        FlushInputs();
        inputFlushTimer_ = 0.0f;
    }

    // Send periodic pings for latency measurement
    pingTimer_ += deltaMs;
    if (pingTimer_ >= PING_INTERVAL_MS) {
        SendPing();
        pingTimer_ = 0.0f;
    }
}

void ClientNetworkSystem::OnEvent(Event& event) {
    ZoneScopedN("ClientNetworkSystem::OnEvent");

    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();

    // Only forward inputs if we're a client
    if (networkService.GetMode() != Services::NetworkMode::Client) {
        return;
    }

    // Convert local events to network inputs and queue them
    Network::NetworkInput input;

    if (auto* e = event.As<MouseButtonPressedEvent>()) {
        input = Network::NetworkInput::FromMouseButtonPressed(*e, nextInputSequence_++);
        QueueInput(input);
    } else if (auto* e = event.As<MouseButtonReleasedEvent>()) {
        input = Network::NetworkInput::FromMouseButtonReleased(*e, nextInputSequence_++);
        QueueInput(input);
    } else if (auto* e = event.As<MouseMovedEvent>()) {
        input = Network::NetworkInput::FromMouseMoved(*e, nextInputSequence_++);
        QueueInput(input);
    } else if (auto* e = event.As<MouseWheelEvent>()) {
        input = Network::NetworkInput::FromMouseWheel(*e, nextInputSequence_++);
        QueueInput(input);
    } else if (auto* e = event.As<KeyPressedEvent>()) {
        input = Network::NetworkInput::FromKeyPressed(*e, nextInputSequence_++);
        QueueInput(input);
    } else if (auto* e = event.As<KeyReleasedEvent>()) {
        input = Network::NetworkInput::FromKeyReleased(*e, nextInputSequence_++);
        QueueInput(input);
    }
}

void ClientNetworkSystem::OnMessage(Message& message) {
    using namespace Services;
    ZoneScopedN("ClientNetworkSystem::OnMessage");

    if (auto* connected = message.As<ServerConnectedMessage>()) {
        ZoneScopedN("Server Connected");
        // Send handshake request
        Network::ByteBuffer buffer(32);

        Network::PacketHeader header;
        header.packetType = static_cast<uint8_t>(Network::PacketType::HandshakeRequest);
        header.flags = 0;
        header.tick = 0;
        header.payloadSize = 4;
        header.Write(buffer);

        Network::HandshakeRequestPacket request;
        request.protocolVersion = Network::PROTOCOL_VERSION;
        request.Write(buffer);

        auto& networkService = Application::GetInstance().GetService<NetworkService>();
        networkService.SendToServer(buffer.Data(), buffer.Size(), true);

        isSynced_ = false;
    } else if (auto* disconnected = message.As<ServerDisconnectedMessage>()) {
        ZoneScopedN("Server Disconnected");

        isSynced_ = false;
        // Clear server-owned entities
        for (Entity entity : serverOwnedEntities_) {
            world->DestroyEntity(entity);
        }
        serverOwnedEntities_.clear();
        networkToLocalEntity_.clear();
    } else if (auto* data = message.As<NetworkDataReceivedMessage>()) {
        ZoneScopedN("Data Received");

        // Parse incoming data
        Network::ByteBuffer buffer(data->data);

        if (buffer.Size() < Network::PacketHeader::SIZE) {
            return;  // Invalid packet
        }

        Network::PacketHeader header;
        header.Read(buffer);

        auto packetType = static_cast<Network::PacketType>(header.packetType);
        switch (packetType) {
            case Network::PacketType::HandshakeResponse:
                HandleHandshakeResponse(buffer);
                break;

            case Network::PacketType::SyncPacket:
                HandleSyncPacket(buffer);
                break;

            case Network::PacketType::EntityCreated:
                HandleEntityCreated(buffer);
                break;

            case Network::PacketType::EntityDestroyed:
                HandleEntityDestroyed(buffer);
                break;

            case Network::PacketType::Pong:
                HandlePong(buffer);
                break;

            default:
                break;
        }
    }
}

void ClientNetworkSystem::QueueInput(const Network::NetworkInput& input) {
    pendingInputs_.push(input);
}

void ClientNetworkSystem::FlushInputs() {
    ZoneScopedN("ClientNetworkSystem::FlushInputs");

    if (pendingInputs_.empty()) {
        return;
    }

    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();
    if (!networkService.IsRunning()) {
        return;
    }

    Network::ByteBuffer buffer(512);

    // Packet header
    Network::PacketHeader header;
    header.packetType = static_cast<uint8_t>(Network::PacketType::InputPacket);
    header.flags = 0;
    header.tick = lastServerTick_;
    header.Write(buffer);

    // Input packet data
    Network::InputPacketData packetData;
    packetData.clientTick = GetTime() * 1000;  // Local timestamp
    packetData.inputCount = static_cast<uint8_t>(
        std::min(pendingInputs_.size(), static_cast<size_t>(Network::MAX_INPUTS_PER_PACKET)));
    packetData.Write(buffer);

    // Serialize inputs
    for (uint8_t i = 0; i < packetData.inputCount && !pendingInputs_.empty(); ++i) {
        pendingInputs_.front().Write(buffer);
        pendingInputs_.pop();
    }

    {
        ZoneScopedN("Sending to Server");
        networkService.SendToServer(buffer.Data(), buffer.Size(), false);  // Unreliable for inputs
    }
}

void ClientNetworkSystem::SendPing() {
    ZoneScopedN("ClientNetworkSystem::SendPing");

    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();
    if (!networkService.IsRunning()) {
        return;
    }

    Network::ByteBuffer buffer(16);

    Network::PacketHeader header;
    header.packetType = static_cast<uint8_t>(Network::PacketType::Ping);
    header.flags = 0;
    header.tick = lastServerTick_;
    header.payloadSize = 4;
    header.Write(buffer);

    lastPingTime_ = static_cast<uint32_t>(GetTime() * 1000);

    Network::PingPacket ping;
    ping.timestamp = lastPingTime_;
    ping.Write(buffer);

    networkService.SendToServer(buffer.Data(), buffer.Size(), false);
}

void ClientNetworkSystem::HandleHandshakeResponse(Network::ByteBuffer& buffer) {
    ZoneScopedN("ClientNetworkSystem::HandleHandshakeResponse");

    Network::HandshakeResponsePacket response;
    response.Read(buffer);

    if (response.protocolVersion != Network::PROTOCOL_VERSION) {
        // Protocol mismatch - should disconnect
        return;
    }

    lastServerTick_ = response.serverTick;

    // Receive full entity state
    for (uint32_t i = 0; i < response.entityCount && buffer.HasRemaining(); ++i) {
        Network::EntitySyncHeader entityHeader;
        entityHeader.Read(buffer);

        Entity entity = GetOrCreateEntity(entityHeader.entityId);
        Network::DeserializeComponentsByMask(entityHeader.componentMask, entity, world, buffer);
    }

    isSynced_ = true;
}

void ClientNetworkSystem::HandleSyncPacket(Network::ByteBuffer& buffer) {
    ZoneScopedN("ClientNetworkSystem::HandleSyncPacket");

    Network::SyncPacketHeader syncHeader;
    syncHeader.Read(buffer);

    lastServerTick_ = syncHeader.serverTick;
    lastProcessedInput_ = syncHeader.lastProcessedInput;

    // Apply entity updates
    for (uint16_t i = 0; i < syncHeader.entityCount && buffer.HasRemaining(); ++i) {
        Network::EntitySyncHeader entityHeader;
        entityHeader.Read(buffer);

        Entity entity = GetOrCreateEntity(entityHeader.entityId);
        Network::DeserializeComponentsByMask(entityHeader.componentMask, entity, world, buffer);
    }
}

void ClientNetworkSystem::HandleEntityCreated(Network::ByteBuffer& buffer) {
    Network::EntityCreatedPacket packet;
    packet.Read(buffer);

    Entity entity = GetOrCreateEntity(packet.entityId);
    Network::DeserializeComponentsByMask(packet.componentMask, entity, world, buffer);
}

void ClientNetworkSystem::HandleEntityDestroyed(Network::ByteBuffer& buffer) {
    Network::EntityDestroyedPacket packet;
    packet.Read(buffer);

    DestroyEntity(packet.entityId);
}

void ClientNetworkSystem::HandlePong(Network::ByteBuffer& buffer) {
    Network::PingPacket pong;
    pong.Read(buffer);

    uint32_t currentTime = static_cast<uint32_t>(GetTime() * 1000);
    latencyMs_ = currentTime - pong.timestamp;
}

Entity ClientNetworkSystem::GetOrCreateEntity(uint32_t networkId) {
    auto it = networkToLocalEntity_.find(networkId);
    if (it != networkToLocalEntity_.end()) {
        return it->second;
    }

    // Create new entity with matching ID
    // For simplicity, we assume Entity IDs match network IDs
    // In a more complex system, you'd want a separate mapping
    Entity entity = static_cast<Entity>(networkId);

    // Check if entity already exists locally
    const auto& livingEntities = world->GetLivingEntities();
    bool exists = std::find(livingEntities.begin(), livingEntities.end(), entity) != livingEntities.end();

    if (!exists) {
        // Entity doesn't exist - create it
        // Note: This is a simplification. In production, you'd want
        // the server to be authoritative about entity creation.
        entity = world->CreateEntity();
    }

    networkToLocalEntity_[networkId] = entity;
    serverOwnedEntities_.insert(entity);

    return entity;
}

void ClientNetworkSystem::DestroyEntity(uint32_t networkId) {
    auto it = networkToLocalEntity_.find(networkId);
    if (it == networkToLocalEntity_.end()) {
        return;
    }

    Entity entity = it->second;
    serverOwnedEntities_.erase(entity);
    networkToLocalEntity_.erase(it);

    world->DestroyEntity(entity);
}

}  // namespace Elysium
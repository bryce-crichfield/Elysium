#include "Systems/ServerNetworkSystem.h"
#include "Application.h"
#include "Component.h"
#include "Entity.h"
#include "Messages/NetworkMessages.h"
#include "Network/ByteBuffer.h"
#include "Scene.h"
#include "Services/NetworkService.h"

namespace Elysium {

ServerNetworkSystem::ServerNetworkSystem(Context context)
    : System(context) {
    // No initialization needed - using free functions now
}

void ServerNetworkSystem::Update(float deltaTime) {
    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();

    // Only process if we're running as server
    if (networkService.GetMode() != Services::NetworkMode::Server) {
        return;
    }

    // Accumulate time and process ticks (scanning happens per-tick, not per-frame)
    AccumulateDeltaTime(deltaTime);
}

void ServerNetworkSystem::AccumulateDeltaTime(float deltaTime) {
    tickAccumulator_ += deltaTime * 1000.0f;  // Convert to milliseconds

    // Process ticks at fixed rate (50ms = 20 Hz)
    while (tickAccumulator_ >= Network::SERVER_TICK_RATE_MS) {
        ProcessTick();
        tickAccumulator_ -= Network::SERVER_TICK_RATE_MS;
    }
}

void ServerNetworkSystem::ProcessTick() {
    currentTick_++;

    // 1. Scan for new entities (once per tick, not per frame)
    ScanAndTrackEntities();

    // 2. Process buffered inputs from clients
    ReplayInputsAsEvents();

    // 3. Send full state to clients that need it
    for (auto& [peer, client] : clients_) {
        if (client.needsFullSync) {
            BroadcastFullState(peer);
            client.needsFullSync = false;
        }
    }

    // 4. Broadcast dirty state to all clients
    SendSyncPacket();
}

void ServerNetworkSystem::ReplayInputsAsEvents() {
    while (!inputBuffer_.empty()) {
        auto& buffered = inputBuffer_.front();

        // Convert network input to local Event and dispatch to scene
        auto event = buffered.input.ToEvent();
        if (event && scene) {
            scene->OnEvent(*event);
        }

        // Track last processed input for this client
        auto it = clients_.find(buffered.peer);
        if (it != clients_.end()) {
            it->second.lastProcessedInput = buffered.input.sequence;
        }

        inputBuffer_.pop();
    }
}

void ServerNetworkSystem::SendSyncPacket() {
    if (clients_.empty()) {
        return;
    }

    // Get dirty entities
    auto dirtyEntities = syncManager_.GetDirtyEntities();
    if (dirtyEntities.empty()) {
        return;
    }

    // Build sync packet
    Network::ByteBuffer buffer(1024);

    // Packet header
    Network::PacketHeader header;
    header.packetType = static_cast<uint8_t>(Network::PacketType::SyncPacket);
    header.flags = 0;
    header.tick = currentTick_;
    // payloadSize filled in later

    // Reserve space for header
    size_t headerPos = buffer.Size();
    header.Write(buffer);

    // Sync packet header
    Network::SyncPacketHeader syncHeader;
    syncHeader.serverTick = currentTick_;
    syncHeader.lastProcessedInput = 0;  // Per-client value handled below
    syncHeader.entityCount = static_cast<uint16_t>(
        std::min(dirtyEntities.size(), static_cast<size_t>(Network::MAX_ENTITIES_PER_SYNC)));
    syncHeader.Write(buffer);

    // Serialize dirty entities
    size_t entitiesSerialized = 0;
    for (Entity entity : dirtyEntities) {
        if (entitiesSerialized >= Network::MAX_ENTITIES_PER_SYNC) {
            break;
        }

        // Get component mask for this entity
        uint32_t componentMask = Network::GetNetworkComponentMask(entity, world);
        if (componentMask == 0) {
            continue;  // No networked components
        }

        // Entity sync header
        Network::EntitySyncHeader entityHeader;
        entityHeader.entityId = static_cast<uint32_t>(entity);
        entityHeader.componentMask = componentMask;
        entityHeader.Write(buffer);

        // Serialize components using the new helper
        Network::SerializeComponentsByMask(componentMask, entity, world, buffer);

        syncManager_.MarkSynced(entity, currentTick_, componentMask);
        entitiesSerialized++;
    }

    // Broadcast to all clients
    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();
    networkService.BroadcastToClients(buffer.Data(), buffer.Size(), false);  // Unreliable for position updates
}

void ServerNetworkSystem::BroadcastFullState(ENetPeer* peer) {
    auto trackedEntities = syncManager_.GetTrackedEntities();

    Network::ByteBuffer buffer(4096);

    // Packet header
    Network::PacketHeader header;
    header.packetType = static_cast<uint8_t>(Network::PacketType::HandshakeResponse);
    header.flags = 0;
    header.tick = currentTick_;
    header.Write(buffer);

    // Handshake response
    Network::HandshakeResponsePacket response;
    response.protocolVersion = Network::PROTOCOL_VERSION;
    response.serverTick = currentTick_;
    response.entityCount = static_cast<uint32_t>(trackedEntities.size());
    response.Write(buffer);

    // Serialize all tracked entities
    for (Entity entity : trackedEntities) {
        uint32_t componentMask = Network::GetNetworkComponentMask(entity, world);

        Network::EntitySyncHeader entityHeader;
        entityHeader.entityId = static_cast<uint32_t>(entity);
        entityHeader.componentMask = componentMask;
        entityHeader.Write(buffer);

        // Serialize components using the new helper
        Network::SerializeComponentsByMask(componentMask, entity, world, buffer);
    }

    // Send to specific client (reliable)
    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();
    networkService.SendToClient(peer, buffer.Data(), buffer.Size(), true);

    // Mark all entities as synced for this tick
    syncManager_.MarkAllSynced(currentTick_);
}

void ServerNetworkSystem::OnMessage(Message& message) {
    using namespace Services;

    if (auto* connected = message.As<ClientConnectedMessage>()) {
        OnClientConnected(connected->peer);
    } else if (auto* disconnected = message.As<ClientDisconnectedMessage>()) {
        OnClientDisconnected(disconnected->peer);
    } else if (auto* data = message.As<NetworkDataReceivedMessage>()) {
        // Parse and handle incoming data
        Network::ByteBuffer buffer(data->data);

        if (buffer.Size() < Network::PacketHeader::SIZE) {
            return;  // Invalid packet
        }

        Network::PacketHeader header;
        header.Read(buffer);

        auto packetType = static_cast<Network::PacketType>(header.packetType);
        switch (packetType) {
            case Network::PacketType::InputPacket:
                ProcessInputPacket(data->peer, buffer);
                break;

            case Network::PacketType::HandshakeRequest:
                // Client requesting connection - send full state
                {
                    auto it = clients_.find(data->peer);
                    if (it != clients_.end()) {
                        it->second.needsFullSync = true;
                    }
                }
                break;

            case Network::PacketType::Ping:
                // Respond with pong
                {
                    Network::PingPacket ping;
                    ping.Read(buffer);

                    Network::ByteBuffer response;
                    Network::PacketHeader pongHeader;
                    pongHeader.packetType = static_cast<uint8_t>(Network::PacketType::Pong);
                    pongHeader.flags = 0;
                    pongHeader.tick = currentTick_;
                    pongHeader.payloadSize = 4;
                    pongHeader.Write(response);
                    ping.Write(response);

                    auto& networkService = Application::GetInstance().GetService<NetworkService>();
                    networkService.SendToClient(data->peer, response.Data(), response.Size(), false);
                }
                break;

            default:
                break;
        }
    }
}

void ServerNetworkSystem::ProcessInputPacket(ENetPeer* peer, Network::ByteBuffer& buffer) {
    Network::InputPacketData packetData;
    packetData.Read(buffer);

    for (uint8_t i = 0; i < packetData.inputCount && buffer.HasRemaining(); ++i) {
        BufferedInput buffered;
        buffered.peer = peer;
        buffered.input.Read(buffer);
        inputBuffer_.push(std::move(buffered));
    }
}

void ServerNetworkSystem::OnClientConnected(ENetPeer* peer) {
    ClientState state;
    state.peer = peer;
    state.lastProcessedInput = 0;
    state.needsFullSync = true;  // Send full world state on first tick
    clients_[peer] = state;
}

void ServerNetworkSystem::OnClientDisconnected(ENetPeer* peer) {
    clients_.erase(peer);
}

void ServerNetworkSystem::MarkEntityDirty(Entity entity) {
    syncManager_.MarkDirty(entity);
}

void ServerNetworkSystem::TrackEntity(Entity entity) {
    syncManager_.TrackEntity(entity);
}

void ServerNetworkSystem::UntrackEntity(Entity entity) {
    // Broadcast destruction to clients before untracking
    if (!clients_.empty()) {
        BroadcastEntityDestroyed(entity);
    }
    syncManager_.UntrackEntity(entity);
}

void ServerNetworkSystem::BroadcastEntityCreated(Entity entity) {
    Network::ByteBuffer buffer(256);

    Network::PacketHeader header;
    header.packetType = static_cast<uint8_t>(Network::PacketType::EntityCreated);
    header.flags = 0;
    header.tick = currentTick_;
    header.Write(buffer);

    uint32_t componentMask = Network::GetNetworkComponentMask(entity, world);

    Network::EntityCreatedPacket packet;
    packet.entityId = static_cast<uint32_t>(entity);
    packet.componentMask = componentMask;
    packet.Write(buffer);

    // Serialize initial component state using the new helper
    Network::SerializeComponentsByMask(componentMask, entity, world, buffer);

    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();
    networkService.BroadcastToClients(buffer.Data(), buffer.Size(), true);  // Reliable
}

void ServerNetworkSystem::BroadcastEntityDestroyed(Entity entity) {
    Network::ByteBuffer buffer(32);

    Network::PacketHeader header;
    header.packetType = static_cast<uint8_t>(Network::PacketType::EntityDestroyed);
    header.flags = 0;
    header.tick = currentTick_;
    header.Write(buffer);

    Network::EntityDestroyedPacket packet;
    packet.entityId = static_cast<uint32_t>(entity);
    packet.Write(buffer);

    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();
    networkService.BroadcastToClients(buffer.Data(), buffer.Size(), true);  // Reliable
}

void ServerNetworkSystem::ScanAndTrackEntities() {
    // Auto-track entities with PositionComponent (the base requirement for networked entities)
    world->Query<PositionComponent>([this](Entity entity, auto& pos) {
        if (!syncManager_.IsTracked(entity)) {
            TrackEntity(entity);
            // Notify clients of new entity
            if (!clients_.empty()) {
                BroadcastEntityCreated(entity);
            }
        }
    });
}

}  // namespace Elysium
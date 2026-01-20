#pragma once

#include <queue>
#include <unordered_set>
#include <vector>
#include "../Network/ComponentSerializer.h"
#include "../Network/NetworkInput.h"
#include "../Network/NetworkProtocol.h"
#include "../System.h"

namespace Elysium {

/**
 * ClientNetworkSystem - Handles client-side network synchronization
 *
 * Responsibilities:
 * - Receive and apply SyncPackets from server
 * - Buffer and send input packets to server
 * - Handle EntityCreated/EntityDestroyed packets
 * - Track known entities from server
 *
 * Future enhancement: Client-side prediction and server reconciliation
 *
 * Usage:
 *   Add to scene when running in client mode:
 *   scene->AddSystem(std::make_unique<ClientNetworkSystem>(context));
 */
class ClientNetworkSystem : public System {
   public:
    explicit ClientNetworkSystem(Context context);
    ~ClientNetworkSystem() override = default;

    void Update(float deltaTime) override;
    void OnEvent(Event& event) override;
    void OnMessage(Message& message) override;

    /**
     * Queue an input to send to the server
     * Called by SceneService when input is captured
     */
    void QueueInput(const Network::NetworkInput& input);

    /**
     * Get the last acknowledged server tick
     */
    uint32_t GetLastServerTick() const { return lastServerTick_; }

    /**
     * Check if connected and synced with server
     */
    bool IsSynced() const { return isSynced_; }

    /**
     * Get round-trip latency estimate (ms)
     */
    uint32_t GetLatency() const { return latencyMs_; }

   private:
    // Packet handlers
    void HandleHandshakeResponse(Network::ByteBuffer& buffer);
    void HandleSyncPacket(Network::ByteBuffer& buffer);
    void HandleEntityCreated(Network::ByteBuffer& buffer);
    void HandleEntityDestroyed(Network::ByteBuffer& buffer);
    void HandlePong(Network::ByteBuffer& buffer);

    // Input sending
    void FlushInputs();
    void SendPing();

    // Entity management
    Entity GetOrCreateEntity(uint32_t networkId);
    void DestroyEntity(uint32_t networkId);

    // Input buffering
    std::queue<Network::NetworkInput> pendingInputs_;
    uint32_t nextInputSequence_ = 1;
    float inputFlushTimer_ = 0.0f;

    // Server state tracking
    uint32_t lastServerTick_ = 0;
    uint32_t lastProcessedInput_ = 0;
    bool isSynced_ = false;

    // Latency measurement
    float pingTimer_ = 0.0f;
    uint32_t lastPingTime_ = 0;
    uint32_t latencyMs_ = 0;

    // Known entities from server (network ID -> local Entity)
    std::unordered_map<uint32_t, Entity> networkToLocalEntity_;
    std::unordered_set<Entity> serverOwnedEntities_;

    // Configuration
    static constexpr float INPUT_FLUSH_INTERVAL_MS = 16.0f;  // ~60 Hz input rate
    static constexpr float PING_INTERVAL_MS = 1000.0f;       // 1 Hz ping rate
};

}  // namespace Elysium

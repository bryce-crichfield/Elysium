#pragma once

#include <queue>
#include <unordered_map>
#include <vector>
#include "ComponentSerializer.h"
#include "EntitySyncManager.h"
#include "NetworkInput.h"
#include "NetworkProtocol.h"
#include "System.h"

// Forward declarations
typedef struct _ENetPeer ENetPeer;

namespace Elysium {

/**
 * ServerNetworkSystem - Handles server-side network synchronization
 *
 * Responsibilities:
 * - Tick accumulator for 50ms server tick rate (20 Hz)
 * - Process buffered client inputs and replay as Events
 * - Track dirty entities via EntitySyncManager
 * - Broadcast SyncPackets to all connected clients
 * - Handle client connect/disconnect lifecycle
 *
 * Usage:
 *   Add to scene when running in server mode:
 *   scene->AddSystem(std::make_unique<ServerNetworkSystem>(context));
 */
class ServerNetworkSystem : public System {
   public:
    explicit ServerNetworkSystem(Context context);
    ~ServerNetworkSystem() override = default;

    void Update(float deltaTime) override;
    void OnMessage(Message& message) override;

    /**
     * Mark an entity as dirty (state changed, needs sync)
     * Call this from other systems when modifying networked components
     */
    void MarkEntityDirty(Entity entity);

    /**
     * Register an entity for network tracking
     * Called automatically for entities with PositionComponent
     */
    void TrackEntity(Entity entity);

    /**
     * Stop tracking an entity
     */
    void UntrackEntity(Entity entity);

    /**
     * Get the current server tick
     */
    uint32_t GetCurrentTick() const { return currentTick_; }

   private:
    // Tick management
    void ProcessTick();
    void AccumulateDeltaTime(float deltaTime);

    // Client management
    void OnClientConnected(ENetPeer* peer);
    void OnClientDisconnected(ENetPeer* peer);

    // Input processing
    void ProcessInputPacket(ENetPeer* peer, Network::ByteBuffer& buffer);
    void ReplayInputsAsEvents();

    // State synchronization
    void BroadcastFullState(ENetPeer* peer);
    void BroadcastDirtyState();
    void SendSyncPacket();

    // Entity lifecycle
    void BroadcastEntityCreated(Entity entity);
    void BroadcastEntityDestroyed(Entity entity);

    // Auto-tracking
    void ScanAndTrackEntities();

    // State
    Network::EntitySyncManager syncManager_;

    // Timing
    float tickAccumulator_ = 0.0f;
    uint32_t currentTick_ = 0;

    // Client state tracking
    struct ClientState {
        ENetPeer* peer = nullptr;
        uint32_t lastProcessedInput = 0;  // For reconciliation
        bool needsFullSync = true;        // Send full state on next tick
    };
    std::unordered_map<ENetPeer*, ClientState> clients_;

    // Buffered inputs from clients (processed each tick)
    struct BufferedInput {
        ENetPeer* peer;
        Network::NetworkInput input;
    };
    std::queue<BufferedInput> inputBuffer_;
};

}  // namespace Elysium

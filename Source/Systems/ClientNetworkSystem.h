#pragma once

#include <unordered_map>
#include <vector>
#include "Core/Future.h"
#include "Entity.h"
#include "Network/Network.h"
#include "Network/Generated.h"
#include "System.h"

namespace Elysium {

class ClientNetworkSystem : public System {
   public:
    explicit ClientNetworkSystem(Context context);
    ~ClientNetworkSystem() override = default;

    void Update(float deltaTime) override;
    void OnEvent(Event& event) override;
    void OnMessage(Message& message) override;

    uint32_t GetLastServerTick() const { return lastServerTick_; }
    bool IsSynced() const { return isSynced_; }
    uint32_t GetLatency() const { return latencyMs_; }

   private:
    void HandleEntityCreated(SerialBuffer& buffer);
    void HandleEntityDestroyed(SerialBuffer& buffer);
    void OnConnectedToServer();

    Entity GetOrCreateEntity(uint32_t networkId);
    void DestroyEntity(uint32_t networkId);

    uint32_t lastServerTick_ = 0;
    bool isSynced_ = false;
    uint32_t latencyMs_ = 0;
    bool waitingForPing_ = false;
    Future<Generated::PingResponse> pingFuture_;

    std::unordered_map<uint32_t, Entity> networkToLocalEntity_;
    std::vector<Entity> serverOwnedEntities_;
};

}  // namespace Elysium

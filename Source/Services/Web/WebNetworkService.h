#pragma once

#include "Core/Log.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/INetworkService.h"

namespace Elysium::Services {

// Browsers can't open UDP sockets, so ENet doesn't exist here. Until there's a WebSocket or
// WebRTC transport, the game runs offline: Start fails and sends go nowhere.
class WebNetworkService : public INetworkService {
   public:
    explicit WebNetworkService(ServiceLocator&) {}

    void Initialize() override {}
    void Shutdown() override {}
    void Update(float) override {}

    bool Start(NetworkConfig) override {
        LOG_WARNING("Network", "Networking isn't available in the browser yet");
        return false;
    }
    bool Stop() override { return false; }

    void SendToServer(const void*, size_t, bool = true) override {}
    void SendToClient(ENetPeer*, const void*, size_t, bool = true) override {}
    void BroadcastToClients(const void*, size_t, bool = true) override {}

    bool IsRunning() const override { return false; }
    NetworkMode GetMode() const override { return NetworkMode::None; }

    uint32_t GetConnectedPeers() const override { return 0; }
    uint32_t GetPacketsSent() const override { return 0; }
    uint32_t GetPacketsReceived() const override { return 0; }
    uint64_t GetBytesSent() const override { return 0; }
    uint64_t GetBytesReceived() const override { return 0; }
};

}  // namespace Elysium::Services

#pragma once

#include <cstdint>
#include <vector>
#include "Interfaces/INetworkService.h"

namespace Elysium::Tests {

// No sockets: records every packet sent so a test can read the replies.
class FakeNetworkService : public Services::INetworkService {
   public:
    void Initialize() override {}
    void Shutdown() override {}
    void Update(float) override {}

    bool Start(NetworkConfig config) override { mode = config.mode; return true; }
    bool Stop() override { return true; }

    void SendToServer(const void* data, size_t length, bool) override { Record(data, length); }
    void SendToClient(ENetPeer*, const void* data, size_t length, bool) override { Record(data, length); }
    void BroadcastToClients(const void* data, size_t length, bool) override { Record(data, length); }

    bool IsRunning() const override { return true; }
    NetworkMode GetMode() const override { return mode; }

    uint32_t GetConnectedPeers() const override { return 0; }
    uint32_t GetPacketsSent() const override { return (uint32_t)sent.size(); }
    uint32_t GetPacketsReceived() const override { return 0; }
    uint64_t GetBytesSent() const override { return 0; }
    uint64_t GetBytesReceived() const override { return 0; }

    NetworkMode mode = NetworkMode::Server;
    std::vector<std::vector<uint8_t>> sent;

   private:
    void Record(const void* data, size_t length) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        sent.emplace_back(bytes, bytes + length);
    }
};

}  // namespace Elysium::Tests

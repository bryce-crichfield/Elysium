#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include "Service.h"

#include "Network/Network.h"

// Forward declare ENet types to avoid pulling enet.h into the header
struct _ENetHost;
struct _ENetPeer;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace Elysium::Services {

class MessageService;

class NetworkService : public Service {
public:
    NetworkService();
    ~NetworkService() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    bool Start(NetworkConfig config);
    bool Stop();

    void SendToServer(const void* data, size_t length, bool reliable = true);
    void SendToClient(ENetPeer* peer, const void* data, size_t length, bool reliable = true);
    void BroadcastToClients(const void* data, size_t length, bool reliable = true);

    bool IsRunning() const { return isRunning_; }
    NetworkMode GetMode() const { return config_.mode; }

    uint32_t GetConnectedPeers() const { return connectedPeers_; }
    uint32_t GetPacketsSent() const { return packetsSent_; }
    uint32_t GetPacketsReceived() const { return packetsReceived_; }
    uint64_t GetBytesSent() const { return bytesSent_; }
    uint64_t GetBytesReceived() const { return bytesReceived_; }

private:
    void OnNetworkData(const NetworkDataMessage& data);
    void NetworkThread();

    bool StartServer(uint16_t port, size_t maxClients);
    bool StartClient(const std::string& address, uint16_t port);

    NetworkConfig config_;

    std::thread networkThread_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> shouldStop_{false};

    mutable std::mutex hostMutex_;

    ENetHost* host_ = nullptr;
    ENetPeer* serverPeer_ = nullptr;

    std::atomic<uint32_t> connectedPeers_{0};
    std::atomic<uint32_t> packetsSent_{0};
    std::atomic<uint32_t> packetsReceived_{0};
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> bytesReceived_{0};

    std::unordered_map<ENetPeer*, NetworkPeer> peerMap_;
};

}  // namespace Elysium::Services

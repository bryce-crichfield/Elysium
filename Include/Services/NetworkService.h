#pragma once

#include "Service.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <cstdint>

// Forward declarations for ENet types (defined in enet/enet.h)
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace Elysium::Services {

class MessageService;

enum class NetworkMode {
    None,
    Server,
    Client
};

struct NetworkConfig {
    NetworkMode mode = NetworkMode::None;
    std::string address = "127.0.0.1";
    uint16_t port = 7777;
    size_t maxClients = 32;
    size_t channelCount = 2;
};

class NetworkService : public Service {
public:
    NetworkService();
    ~NetworkService() override;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void ImGui() override;

    // Network control
    void StartServer(uint16_t port = 7777, size_t maxClients = 32);
    void StartClient(const std::string& address = "127.0.0.1", uint16_t port = 7777);
    void Stop();

    // Send data (thread-safe)
    void SendToServer(const void* data, size_t length, bool reliable = true);
    void SendToClient(ENetPeer* peer, const void* data, size_t length, bool reliable = true);
    void BroadcastToClients(const void* data, size_t length, bool reliable = true);

    // Status
    bool IsRunning() const { return isRunning_; }
    NetworkMode GetMode() const { return config_.mode; }

private:
    void NetworkThread();
    void ProcessServerEvents();
    void ProcessClientEvents();

    NetworkConfig config_;

    ENetHost* host_ = nullptr;
    ENetPeer* serverPeer_ = nullptr; // For client mode

    std::thread networkThread_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> shouldStop_{false};
    
    mutable std::mutex hostMutex_; // Protects ENet host operations

    // Stats for ImGui
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> bytesReceived_{0};
    std::atomic<uint32_t> packetsReceived_{0};
    std::atomic<uint32_t> packetsSent_{0};
    std::atomic<uint32_t> connectedPeers_{0};
};

} // namespace Elysium::Services
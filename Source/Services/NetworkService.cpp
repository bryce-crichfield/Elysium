// Prevent Windows headers from declaring symbols that conflict with raylib
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define NOGDI       // Prevents Rectangle declaration in wingdi.h
#define NOUSER      // Prevents CloseWindow, ShowCursor in winuser.h
#endif

#include <enet/enet.h>

#ifdef _WIN32
#undef NOGDI
#undef NOUSER
#endif

// Now include raylib via Application.h
#include "Application.h"
#include "Services/NetworkService.h"
#include "Services/MessageService.h"
#include "Services/LogService.h"

#include "Messages/NetworkMessages.h"

namespace Elysium::Services {

NetworkService::NetworkService()
{
    name_ = "NetworkService";
    hasUi_ = true;
}

NetworkService::~NetworkService() {
    Shutdown();
}

void NetworkService::Initialize() {
    if (enet_initialize() != 0) {
        LOG_ERROR("Network", "Failed to initialize ENet");
        return;
    }
}

void NetworkService::Shutdown() {
    Stop();
    
    if (host_) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }

    enet_deinitialize();
}

void NetworkService::Update(float deltaTime) {
    // Most work happens on the network thread
    // This is just for any main-thread coordination if needed
}

void NetworkService::StartServer(uint16_t port, size_t maxClients) {
    if (isRunning_) {
        LOG_WARNING("Network", "Network already running");
        return;
    }

    config_.mode = NetworkMode::Server;
    config_.port = port;
    config_.maxClients = maxClients;

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    std::lock_guard<std::mutex> lock(hostMutex_);
    host_ = enet_host_create(&address, maxClients, config_.channelCount, 0, 0);

    if (!host_) {
        LOG_ERROR("Network", "Failed to create ENet server");
        return;
    }

    isRunning_ = true;
    shouldStop_ = false;
    networkThread_ = std::thread(&NetworkService::NetworkThread, this);

    auto& messageService = Application::GetInstance().GetService<Elysium::Services::MessageService>();
    messageService.Post<NetworkServerStartedMessage>(port, maxClients);
    LOG_INFOF("Network", "Server started on port %d", port);
}

void NetworkService::StartClient(const std::string& address, uint16_t port) {
    if (isRunning_) {
        LOG_WARNING("Network", "Network already running");
        return;
    }

    config_.mode = NetworkMode::Client;
    config_.address = address;
    config_.port = port;

    std::lock_guard<std::mutex> lock(hostMutex_);
    host_ = enet_host_create(nullptr, 1, config_.channelCount, 0, 0);

    if (!host_) {
        LOG_ERROR("Network", "Failed to create ENet client");
        return;
    }

    ENetAddress serverAddress;
    enet_address_set_host(&serverAddress, address.c_str());
    serverAddress.port = port;

    serverPeer_ = enet_host_connect(host_, &serverAddress, config_.channelCount, 0);

    if (!serverPeer_) {
        LOG_ERROR("Network", "Failed to create connection peer");
        enet_host_destroy(host_);
        host_ = nullptr;
        return;
    }

    isRunning_ = true;
    shouldStop_ = false;
    networkThread_ = std::thread(&NetworkService::NetworkThread, this);

    LOG_INFOF("Network", "Client connecting to %s:%d", address.c_str(), port);
}

void NetworkService::Stop() {
    if (!isRunning_) return;

    shouldStop_ = true;
    
    if (networkThread_.joinable()) {
        networkThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(hostMutex_);
        
        if (config_.mode == NetworkMode::Client && serverPeer_) {
            enet_peer_disconnect(serverPeer_, 0);
            serverPeer_ = nullptr;
        }
        
        if (host_) {
            enet_host_destroy(host_);
            host_ = nullptr;
        }
    }

    isRunning_ = false;
    connectedPeers_ = 0;
    auto& messageService = Application::GetInstance().GetService<Elysium::Services::MessageService>();

    messageService.Post<NetworkStoppedMessage>();
    LOG_INFO("Network", "Network stopped");
}

void NetworkService::NetworkThread() {
    ENetEvent event;

    while (!shouldStop_) {
        auto& messageService = Application::GetInstance().GetService<Elysium::Services::MessageService>();

        std::lock_guard<std::mutex> lock(hostMutex_);
        
        if (!host_) break;

        while (enet_host_service(host_, &event, 10) > 0) {
            if (config_.mode == NetworkMode::Server) {
                ProcessServerEvents();
            } else {
                ProcessClientEvents();
            }

            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    if (config_.mode == NetworkMode::Server) {
                        connectedPeers_++;
                        messageService.Post<ClientConnectedMessage>(event.peer);
                        LOG_INFOF("Network", "Client connected from %u:%u",
                                  event.peer->address.host, event.peer->address.port);
                    } else {
                        connectedPeers_ = 1;
                        messageService.Post<ServerConnectedMessage>();
                        LOG_INFO("Network", "Connected to server");
                    }
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    packetsReceived_++;
                    bytesReceived_ += event.packet->dataLength;

                    messageService.Post<NetworkDataReceivedMessage>(
                        event.peer,
                        event.packet->data,
                        event.packet->dataLength,
                        event.channelID
                    );

                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    if (config_.mode == NetworkMode::Server) {
                        connectedPeers_--;
                        messageService.Post<ClientDisconnectedMessage>(event.peer);
                        LOG_INFO("Network", "Client disconnected");
                    } else {
                        connectedPeers_ = 0;
                        messageService.Post<ServerDisconnectedMessage>();
                        LOG_INFO("Network", "Disconnected from server");
                    }
                    event.peer->data = nullptr;
                    break;

                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }
}

void NetworkService::ProcessServerEvents() {
    // Server-specific event processing if needed
}

void NetworkService::ProcessClientEvents() {
    // Client-specific event processing if needed
}

void NetworkService::SendToServer(const void* data, size_t length, bool reliable) {
    if (config_.mode != NetworkMode::Client || !serverPeer_) return;

    std::lock_guard<std::mutex> lock(hostMutex_);
    
    ENetPacket* packet = enet_packet_create(
        data,
        length,
        reliable ? ENET_PACKET_FLAG_RELIABLE : 0
    );

    if (enet_peer_send(serverPeer_, 0, packet) == 0) {
        packetsSent_++;
        bytesSent_ += length;
    }
    
    enet_host_flush(host_);
}

void NetworkService::SendToClient(ENetPeer* peer, const void* data, size_t length, bool reliable) {
    if (config_.mode != NetworkMode::Server || !peer) return;

    std::lock_guard<std::mutex> lock(hostMutex_);
    
    ENetPacket* packet = enet_packet_create(
        data,
        length,
        reliable ? ENET_PACKET_FLAG_RELIABLE : 0
    );

    if (enet_peer_send(peer, 0, packet) == 0) {
        packetsSent_++;
        bytesSent_ += length;
    }
    
    enet_host_flush(host_);
}

void NetworkService::BroadcastToClients(const void* data, size_t length, bool reliable) {
    if (config_.mode != NetworkMode::Server) return;

    std::lock_guard<std::mutex> lock(hostMutex_);
    
    ENetPacket* packet = enet_packet_create(
        data,
        length,
        reliable ? ENET_PACKET_FLAG_RELIABLE : 0
    );

    enet_host_broadcast(host_, 0, packet);
    
    packetsSent_++;
    bytesSent_ += length;
    
    enet_host_flush(host_);
}

void NetworkService::ImGui() {
    if (!isVisible_) return;

    ImGui::Begin(name_.c_str(), &isVisible_);

    // Status
    ImGui::Text("Mode: %s", 
        config_.mode == NetworkMode::Server ? "Server" :
        config_.mode == NetworkMode::Client ? "Client" : "None");
    ImGui::Text("Running: %s", isRunning_.load() ? "Yes" : "No");
    ImGui::Text("Connected Peers: %u", connectedPeers_.load());

    ImGui::Separator();

    // Stats
    ImGui::Text("Packets Sent: %u", packetsSent_.load());
    ImGui::Text("Packets Received: %u", packetsReceived_.load());
    ImGui::Text("Bytes Sent: %llu", bytesSent_.load());
    ImGui::Text("Bytes Received: %llu", bytesReceived_.load());

    ImGui::Separator();

    // Controls
    if (!isRunning_) {
        static char addressBuf[128] = "127.0.0.1";
        static int port = 7777;

        ImGui::InputText("Address", addressBuf, sizeof(addressBuf));
        ImGui::InputInt("Port", &port);

        if (ImGui::Button("Start Server")) {
            StartServer(port);
        }
        ImGui::SameLine();
        if (ImGui::Button("Start Client")) {
            StartClient(addressBuf, port);
        }
    } else {
        if (ImGui::Button("Stop")) {
            Stop();
        }
    }

    ImGui::End();
}

} // namespace Elysium::Services
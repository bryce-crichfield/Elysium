// Prevent Windows headers from declaring symbols that conflict with raylib
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define NOGDI   // Prevents Rectangle declaration in wingdi.h
#define NOUSER  // Prevents CloseWindow, ShowCursor in winuser.h
#endif

#include <enet/enet.h>

#ifdef _WIN32
#undef NOGDI
#undef NOUSER
#endif

// Now include raylib via Application.h
#include "Core/Application.h"
#include "Services/LogService.h"
#include "Services/MessageService.h"
#include "Services/NetworkService.h"
#include "Services/SceneService.h"

#include "Messages/NetworkMessages.h"
#include "Messages/SceneMessages.h"
#include "Network/ByteBuffer.h"
#include "Network/NetworkProtocol.h"

#include <tracy/Tracy.hpp>

namespace Elysium::Services {

NetworkService::NetworkService() {
    name_ = "NetworkService";
}

NetworkService::~NetworkService() {
    Shutdown();
}

void NetworkService::Initialize() {
    if (enet_initialize() != 0) {
        LOG_ERROR("Network", "Failed to initialize ENet");
        return;
    }

    // Subscribe to scene changes to broadcast to clients (server mode)
    auto& messageService = Application::GetInstance().GetService<MessageService>();
    messageService.Subscribe<SceneChangedMessage>(this, [this](const SceneChangedMessage& msg) {
        OnSceneChanged(msg);
    });

    // Subscribe to network data to handle scene changes (client mode)
    messageService.Subscribe<NetworkDataReceivedMessage>(this, [this](const NetworkDataReceivedMessage& msg) {
        OnNetworkDataReceived(msg);
    });
}

void NetworkService::Shutdown() {
    // Unsubscribe from messages
    auto& messageService = Application::GetInstance().GetService<MessageService>();
    messageService.UnsubscribeAll(this);

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
    if (!isRunning_)
        return;

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
    tracy::SetThreadName("Network Thread");
    ENetEvent event;

    while (!shouldStop_) {
        auto& messageService = Application::GetInstance().GetService<Elysium::Services::MessageService>();

        {
            ZoneScopedN("NetworkService::Poll");
            std::lock_guard<std::mutex> lock(hostMutex_);

            if (!host_)
                break;

            while (enet_host_service(host_, &event, 0) > 0) {
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
                            event.channelID);

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

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void NetworkService::ProcessServerEvents() {
    // Server-specific event processing if needed
}

void NetworkService::ProcessClientEvents() {
    // Client-specific event processing if needed
}

void NetworkService::SendToServer(const void* data, size_t length, bool reliable) {
    if (config_.mode != NetworkMode::Client || !serverPeer_)
        return;

    std::lock_guard<std::mutex> lock(hostMutex_);

    ENetPacket* packet = enet_packet_create(
        data,
        length,
        reliable ? ENET_PACKET_FLAG_RELIABLE : 0);

    if (enet_peer_send(serverPeer_, 0, packet) == 0) {
        packetsSent_++;
        bytesSent_ += length;
    }
}

void NetworkService::SendToClient(ENetPeer* peer, const void* data, size_t length, bool reliable) {
    if (config_.mode != NetworkMode::Server || !peer)
        return;

    std::lock_guard<std::mutex> lock(hostMutex_);

    ENetPacket* packet = enet_packet_create(
        data,
        length,
        reliable ? ENET_PACKET_FLAG_RELIABLE : 0);

    if (enet_peer_send(peer, 0, packet) == 0) {
        packetsSent_++;
        bytesSent_ += length;
    }
}

void NetworkService::BroadcastToClients(const void* data, size_t length, bool reliable) {
    if (config_.mode != NetworkMode::Server)
        return;

    std::lock_guard<std::mutex> lock(hostMutex_);

    ENetPacket* packet = enet_packet_create(
        data,
        length,
        reliable ? ENET_PACKET_FLAG_RELIABLE : 0);

    enet_host_broadcast(host_, 0, packet);

    packetsSent_++;
    bytesSent_ += length;
}

void NetworkService::OnSceneChanged(const SceneChangedMessage& msg) {
    LOG_INFO("Network", "OnSceneChanged");
    // Only broadcast if we're the server
    if (config_.mode != NetworkMode::Server || !isRunning_) {
        return;
    }

    Network::ByteBuffer buffer(128);

    // Packet header
    Network::PacketHeader header;
    header.packetType = static_cast<uint8_t>(Network::PacketType::SceneChange);
    header.flags = 0;
    header.tick = 0;  // Scene changes aren't tick-sensitive
    header.Write(buffer);

    // Scene change packet
    Network::SceneChangePacket packet;
    packet.operation = msg.operation;
    packet.SetSceneName(msg.sceneName);
    packet.Write(buffer);

    // Broadcast reliably to all clients
    BroadcastToClients(buffer.Data(), buffer.Size(), true);

    LOG_INFOF("Network", "Broadcast scene change: %s (op=%d)",
              msg.sceneName.c_str(), static_cast<int>(msg.operation));
}

void NetworkService::OnNetworkDataReceived(const NetworkDataReceivedMessage& msg) {
    // Only handle on client
    if (config_.mode != NetworkMode::Client) {
        return;
    }

    // Parse packet header to check for scene change
    Network::ByteBuffer buffer(msg.data);

    if (buffer.Size() < Network::PacketHeader::SIZE) {
        return;
    }

    Network::PacketHeader header;
    header.Read(buffer);

    // Only handle SceneChange packets - let other packets pass through to systems
    if (static_cast<Network::PacketType>(header.packetType) != Network::PacketType::SceneChange) {
        return;
    }

    Network::SceneChangePacket packet;
    packet.Read(buffer);

    std::string sceneName = packet.GetSceneName();
    LOG_INFOF("Network", "Server requested scene change: %s (op=%d)",
              sceneName.c_str(), static_cast<int>(packet.operation));

    auto& sceneService = Application::GetInstance().GetService<SceneService>();

    switch (packet.operation) {
        case Network::SceneChangeOp::Push:
            sceneService.Push(sceneName);
            break;

        case Network::SceneChangeOp::Pop:
            sceneService.Pop();
            break;

        case Network::SceneChangeOp::Replace:
            sceneService.Replace(sceneName);
            break;

        case Network::SceneChangeOp::Clear:
            sceneService.Clear();
            if (!sceneName.empty()) {
                sceneService.Push(sceneName);
            }
            break;
    }
}

}  // namespace Elysium::Services
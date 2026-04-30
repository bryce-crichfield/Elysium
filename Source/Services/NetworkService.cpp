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
#include "Core/Common.h"
#include "Services/LogService.h"
#include "Services/MessageService.h"
#include "Services/NetworkService.h"

#include <tracy/Tracy.hpp>
#include "Network/Generated.h"

namespace Elysium::Services {

NetworkService::NetworkService(ServiceRegistry& registry) : Service(registry) {
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
}

bool NetworkService::Start(NetworkConfig config) {
    if (isRunning_) {
        LOG_ERROR("Network", "Network already running");
        return false;
    }

    config_ = config;

    if (config.mode == NetworkMode::Server) {
        return StartServer(config.port, config.maxClients);
    } 
    
    if (config.mode == NetworkMode::Client) {
        return StartClient(config.address, config.port);
    } 
    
    LOG_ERROR("Network", "Network mode is None; not starting");
    return false;
}

bool NetworkService::StartServer(uint16_t port, size_t maxClients) {
    if (isRunning_) {
        LOG_ERROR("Network", "Network already running");
        return false;
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
        return false;
    }

    isRunning_ = true;
    shouldStop_ = false;
    networkThread_ = std::thread(&NetworkService::NetworkThread, this);

    LOG_INFOF("Network", "Server started on port %d", port);

    using namespace Elysium::Generated;
    auto& invoke = registry_.GetService<InvokeService>();

    invoke.Register<Ping>(
        std::function<PingResponse(NetworkPeer, const PingRequest&)>(
        [this](NetworkPeer peer, const PingRequest& req) -> PingResponse {
            LOG_INFOF("ServerNet", "Ping from peer=%zu clientTick=%u", peer, req.clientTick);
            PingResponse resp;
            resp.serverTick = static_cast<uint32_t>(GetTime() * 1000);
            resp.echoClientTick = req.clientTick;
            return resp;
        }));

    invoke.Register<SpawnPlayer>(
        std::function<SpawnPlayerResponse(NetworkPeer, const SpawnPlayerRequest&)>(
        [](NetworkPeer peer, const SpawnPlayerRequest& req) -> SpawnPlayerResponse {
            LOG_INFOF("ServerNet", "SpawnPlayer from peer=%zu scene=%u player='%s'",
                      peer, req.sceneId, req.playerName.c_str());
            SpawnPlayerResponse resp;
            resp.entityId = 1;
            resp.success = true;
            return resp;
        }));

    invoke.Register<Test>(
        std::function<TestResponse(NetworkPeer, const TestRequest&)>(
        [](NetworkPeer peer, const TestRequest& req) -> TestResponse {
            LOG_INFOF("ServerNet", "Test request from peer=%zu value1=%u value2='%s'",
                      peer, req.value1, req.value2.c_str());
            TestResponse resp;
            resp.result = "Hello, " + req.value2 + "! Your value1 doubled is " + std::to_string(req.value1 * 2);
            return resp;
        }));

    LOG_INFO("ServerNet", "Registered Ping and SpawnPlayer handlers");

    return true;
}

bool NetworkService::StartClient(const std::string& address, uint16_t port) {
    if (isRunning_) {
        LOG_ERROR("Network", "Network already running");
        return false;
    }

    config_.mode = NetworkMode::Client;
    config_.address = address;
    config_.port = port;

    std::lock_guard<std::mutex> lock(hostMutex_);
    host_ = enet_host_create(nullptr, 1, config_.channelCount, 0, 0);

    if (!host_) {
        LOG_ERROR("Network", "Failed to create ENet client");
        return false;
    }

    ENetAddress serverAddress;
    enet_address_set_host(&serverAddress, address.c_str());
    serverAddress.port = port;

    serverPeer_ = enet_host_connect(host_, &serverAddress, config_.channelCount, 0);

    if (!serverPeer_) {
        LOG_ERROR("Network", "Failed to create connection peer");
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }

    isRunning_ = true;
    shouldStop_ = false;
    networkThread_ = std::thread(&NetworkService::NetworkThread, this);

    LOG_INFOF("Network", "Client connecting to %s:%d", address.c_str(), port);
    return true;
}

bool NetworkService::Stop() {
    if (!isRunning_)
        return false;

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
    auto& messageService = registry_.GetService<MessageService>();
    messageService.Post<NetworkStoppedMessage>();
    LOG_INFO("Network", "Network stopped");
    return true;
}

void NetworkService::NetworkThread() {
    tracy::SetThreadName("Network Thread");
    ENetEvent event;

    while (!shouldStop_) {
        auto& messageService = registry_.GetService<MessageService>();
        {
            ZoneScopedN("NetworkService::Poll");
            std::lock_guard<std::mutex> lock(hostMutex_);

            if (!host_) { break; }

            while (enet_host_service(host_, &event, 0) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        if (config_.mode == NetworkMode::Server) {
                            connectedPeers_++;
                            messageService.Post<NetworkConnectedMessage>(NetworkMode::Server, static_cast<void*>(event.peer));
                            LOG_INFOF("Network", "Client connected from %u:%u",
                                      event.peer->address.host, event.peer->address.port);
                        } else {
                            connectedPeers_ = 1;
                            messageService.Post<NetworkConnectedMessage>(NetworkMode::Client);
                            LOG_INFO("Network", "Connected to server");
                        }
                        break;

                    case ENET_EVENT_TYPE_RECEIVE: {
                        packetsReceived_++;
                        bytesReceived_ += event.packet->dataLength;

                        auto peerId = peerMap_.find(event.peer);
                        NetworkPeer peer = (peerId != peerMap_.end()) ? peerId->second : INVALID_PEER;

                        // Copy data BEFORE destroying the packet
                        std::vector<uint8_t> data(
                            event.packet->data,
                            event.packet->data + event.packet->dataLength);

                        enet_packet_destroy(event.packet);  // safe to destroy now, we own the copy

                        messageService.Post<NetworkDataMessage>(peer, std::move(data), event.channelID);
                        break;
                    }

                    case ENET_EVENT_TYPE_DISCONNECT:
                        if (config_.mode == NetworkMode::Server) {
                            connectedPeers_--;
                            messageService.Post<NetworkDisconnectedMessage>(NetworkMode::Server, static_cast<void*>(event.peer));
                            LOG_INFO("Network", "Client disconnected");
                        } else {
                            connectedPeers_ = 0;
                            messageService.Post<NetworkDisconnectedMessage>(NetworkMode::Client);
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


void NetworkService::OnNetworkData(const NetworkDataMessage& msg) {
    // InvokeService subscribes to NetworkDataMessage and handles InvokeRequest/InvokeResponse.
    // NetworkService doesn't need to dispatch these specific messages anymore.
}

}  // namespace Elysium::Services
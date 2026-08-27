#pragma once

#include <cstdint>
#include "Network/Network.h"

struct _ENetPeer;
typedef struct _ENetPeer ENetPeer;

namespace Elysium::Services {

class INetworkService {
   public:
    virtual ~INetworkService() = default;

    virtual bool Start(NetworkConfig config) = 0;
    virtual bool Stop() = 0;

    virtual void SendToServer(const void* data, size_t length, bool reliable = true) = 0;
    virtual void SendToClient(ENetPeer* peer, const void* data, size_t length, bool reliable = true) = 0;
    virtual void BroadcastToClients(const void* data, size_t length, bool reliable = true) = 0;

    virtual bool IsRunning() const = 0;
    virtual NetworkMode GetMode() const = 0;

    virtual uint32_t GetConnectedPeers() const = 0;
    virtual uint32_t GetPacketsSent() const = 0;
    virtual uint32_t GetPacketsReceived() const = 0;
    virtual uint64_t GetBytesSent() const = 0;
    virtual uint64_t GetBytesReceived() const = 0;
};

}  // namespace Elysium::Services

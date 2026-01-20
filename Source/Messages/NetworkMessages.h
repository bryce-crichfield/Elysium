#pragma once

#include <cstdint>
#include <vector>
#include "Message.h"

// Forward declaration for ENet type (defined in enet/enet.h)
typedef struct _ENetPeer ENetPeer;

namespace Elysium::Services {

struct NetworkServerStartedMessage : public Message {
    uint16_t port;
    size_t maxClients;
    NetworkServerStartedMessage(uint16_t p, size_t max) : port(p), maxClients(max) {}
};

struct NetworkStoppedMessage : public Message {};

struct ClientConnectedMessage : public Message {
    ENetPeer* peer;
    ClientConnectedMessage(ENetPeer* p) : peer(p) {}
};

struct ClientDisconnectedMessage : public Message {
    ENetPeer* peer;
    ClientDisconnectedMessage(ENetPeer* p) : peer(p) {}
};

struct ServerConnectedMessage : public Message {};
struct ServerDisconnectedMessage : public Message {};

struct NetworkDataReceivedMessage : public Message {
    ENetPeer* peer;
    std::vector<uint8_t> data;
    uint8_t channelID;

    NetworkDataReceivedMessage(ENetPeer* p, const void* d, size_t len, uint8_t ch)
        : peer(p), data(static_cast<const uint8_t*>(d), static_cast<const uint8_t*>(d) + len), channelID(ch) {}
};

}  // namespace Elysium::Services
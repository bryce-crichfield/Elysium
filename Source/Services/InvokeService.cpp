#include "Services/InvokeService.h"
#include "Services/LogService.h"
#include "Services/MessageService.h"
#include "Services/NetworkService.h"

namespace Elysium::Services {

InvokeService::InvokeService(ServiceRegistry& registry) : Service(registry) {
    name_ = "InvokeService";
}

void InvokeService::Initialize() {
    auto& messageService = registry_.GetService<MessageService>();

    messageService.Subscribe<NetworkDataMessage>(this,
        [this](const NetworkDataMessage& msg) {
            OnNetworkData(msg);
        });
}

void InvokeService::Shutdown() {
    auto& messageService = registry_.GetService<MessageService>();
    messageService.UnsubscribeAll(this);
    invokeHandlers_.clear();
    pending_.clear();
}

void InvokeService::Update(float deltaTime) {
    // Work happens via message subscriptions
}

void InvokeService::OnNetworkData(const NetworkDataMessage& msg) {
    if (msg.channel != static_cast<uint8_t>(NetworkChannel::Reliable)) {
        LOG_WARNINGF("Invoke", "Ignoring invoke packet on channel %d", msg.channel);
        return;
    }
    
    SerialBuffer buffer(msg.data.data(), msg.data.size());  // reads from owned vector

    if (buffer.Size() < PacketHeader::SIZE) return;

    PacketHeader header{};
    PacketHeader::Read(header, buffer);

    auto packetType = static_cast<PacketType>(header.packetType);

    switch (packetType) {
        case PacketType::InvokeRequest: {
            if (buffer.Remaining() < InvokeHeader::SIZE) return;

            InvokeHeader invokeHeader{};
            InvokeHeader::Read(invokeHeader, buffer);

            auto it = invokeHandlers_.find(invokeHeader.procedureId);
            if (it == invokeHandlers_.end()) {
                LOG_ERRORF("Invoke", "No handler for procedure %u", invokeHeader.procedureId);
                return;
            }

            auto& entry = it->second;

            try {
                SerializableObject request = entry.deserializeRequest(buffer);
                SerializableObject response = entry.invoke(msg.peer, request);
                SendInvokeResponse(msg.peer, invokeHeader.invokeId, invokeHeader.procedureId, response);
            } catch (const std::exception& e) {
                LOG_ERRORF("Invoke", "Malformed payload for procedure %u: %s",
                           invokeHeader.procedureId, e.what());
            }
            break;
        }

        case PacketType::InvokeResponse: {
            if (buffer.Remaining() < InvokeHeader::SIZE) return;

            InvokeHeader invokeHeader{};
            InvokeHeader::Read(invokeHeader, buffer);

            auto it = pending_.find(invokeHeader.invokeId);
            if (it == pending_.end()) {
                LOG_ERRORF("Invoke", "No pending invoke for id %u", invokeHeader.invokeId);
                return;
            }

            it->second.deserializeAndResolve(buffer);
            pending_.erase(it);
            break;
        }

        default:
            break;
    }
}

void InvokeService::SendPacket(NetworkPeer peer, const SerialBuffer& buffer) {
    auto& networkService = registry_.GetService<NetworkService>();
    if (networkService.GetMode() == NetworkMode::Client) {
        networkService.SendToServer(buffer.Data(), buffer.Size());
    } else {
        // Server sending to a specific client — for now broadcast
        // TODO: route to specific peer when peer mapping is available
        networkService.BroadcastToClients(buffer.Data(), buffer.Size());
    }
}

void InvokeService::SendInvokeResponse(NetworkPeer peer, uint32_t invokeId, InvokeMethodId methodId, const SerializableObject& response) {
    PacketWriter writer;
    writer.BeginPacket(PacketType::InvokeResponse, currentTick_)
          .WriteInvokeHeader(invokeId, methodId);

    // Serialize the response directly into the writer's buffer after the header
    SerialBuffer respBuf;
    response.Write(respBuf);
    writer.WriteBytes(respBuf.Data(), respBuf.Size());

    auto buffer = writer.Build();
    SendPacket(peer, buffer);
}

}  // namespace Elysium::Services

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "Core/Future.h"
#include "Core/Serial.h"
#include "Interfaces/IInvokeService.h"
#include "Network/Network.h"
#include "Services/Service.h"

namespace Elysium::Services {

/**
 * Type-erased handler entry.
 * Captures concrete Request/Response types via lambdas so they can
 * all live in a single map keyed by method ID.
 */
struct InvokeHandler {
    IInvokeService::RawHandler invoke;
    IInvokeService::DeserializeRequestFunc deserializeRequest;
    std::function<void(const SerializableObject&, SerialBuffer&)> serializeResponse;
};

/**
 * Type-erased pending invoke.
 * When a remote response arrives, we deserialize the concrete Response
 * and resolve the Future without knowing the type at the call site.
 */
struct PendingInvoke {
    IInvokeService::ResolveResponseFunc deserializeAndResolve;
};

class InvokeService : public Elysium::Service, public IInvokeService {
public:
    InvokeService(ServiceLocator& registry);
    ~InvokeService() = default;
    InvokeService(const InvokeService&) = delete;
    InvokeService& operator=(const InvokeService&) = delete;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    void RegisterRaw(InvokeMethodId methodId, RawHandler invoke, DeserializeRequestFunc deserializeRequest) override;
    SerializableObject InvokeLocalRaw(InvokeMethodId methodId, const SerializableObject& request) override;
    void InvokeRemoteRaw(NetworkPeer peer, InvokeMethodId methodId, const SerializableObject& request,
                          ResolveResponseFunc resolveResponse) override;

private:
    void OnNetworkData(const NetworkDataMessage& msg);
    void SendPacket(NetworkPeer peer, const SerialBuffer& buffer);
    void SendInvokeResponse(NetworkPeer peer, uint32_t invokeId, InvokeMethodId methodId, const SerializableObject& response);

    uint32_t nextInvokeId_ = 1;
    uint32_t currentTick_ = 0;
    std::unordered_map<InvokeMethodId, InvokeHandler> invokeHandlers_;
    std::unordered_map<uint32_t, PendingInvoke> pending_;
};

}  // namespace Elysium::Services

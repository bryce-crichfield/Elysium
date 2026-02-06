#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "Core/Future.h"
#include "Core/Serial.h"
#include "Network/Network.h"
#include "Services/Service.h"

namespace Elysium::Services {

template <typename T>
struct InvokeMethod;

using InvokeFunc = std::function<SerializableObject(NetworkPeer, const SerializableObject&)>;
/**
 * Type-erased handler entry.
 * Captures concrete Request/Response types via lambdas so they can
 * all live in a single map keyed by method ID.
 */
struct InvokeHandler {
    using DeserializeRequestFunc = std::function<SerializableObject(SerialBuffer&)>;
    using SerializeResponseFunc = std::function<void(const SerializableObject&, SerialBuffer&)>;

    InvokeFunc invoke;

    DeserializeRequestFunc deserializeRequest;
    SerializeResponseFunc serializeResponse;
};

/**
 * Type-erased pending invoke.
 * When a remote response arrives, we deserialize the concrete Response
 * and resolve the Future without knowing the type at the call site.
 */
struct PendingInvoke {
    std::function<void(SerialBuffer&)> deserializeAndResolve;
};

class InvokeService : public Elysium::Service {
public:
    InvokeService();
    ~InvokeService() = default;
    InvokeService(const InvokeService&) = delete;
    InvokeService& operator=(const InvokeService&) = delete;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    /**
     * Register an RPC handler for a given method ID.
     * Request and Response must satisfy the Serializable concept.
     */
    template <Serializable Request, Serializable Response>
    void RegisterInvokeHandler(InvokeMethodId methodId,
                  std::function<Response(NetworkPeer, const Request&)> handler) {
        InvokeHandler entry;

        entry.deserializeRequest = [](SerialBuffer& buf) -> SerializableObject {
            Request req{};
            Request::Read(req, buf);
            return SerializableObject(std::move(req));
        };

        entry.invoke = [handler](NetworkPeer peer, const SerializableObject& reqObj) -> SerializableObject {
            // Round-trip: serialize the type-erased request, then deserialize as concrete type
            SerialBuffer tmpBuf;
            reqObj.Write(tmpBuf);
            Request req{};
            Request::Read(req, tmpBuf);
            Response resp = handler(peer, req);
            return SerializableObject(std::move(resp));
        };

        entry.serializeResponse = [](const SerializableObject& resp, SerialBuffer& buf) {
            resp.Write(buf);
        };

        registry_[methodId] = std::move(entry);
    }

    /**
     * Invoke a locally registered procedure (no network round-trip).
     */
    template <Serializable Request, Serializable Response>
    Response InvokeLocal(InvokeMethodId methodId, const Request& request) {
        auto it = registry_.find(methodId);
        if (it == registry_.end()) {
            throw std::runtime_error("InvokeService: no handler for method " + std::to_string(methodId));
        }

        auto& entry = it->second;
        SerializableObject reqObj(request);
        SerializableObject respObj = entry.invoke(LOCAL_PEER, reqObj);

        // Round-trip to recover the concrete Response
        SerialBuffer tmpBuf;
        respObj.Write(tmpBuf);
        Response resp{};
        Response::Read(resp, tmpBuf);
        return resp;
    }

    // ── Type-based API (uses InvokeMethod<T> specializations) ──────────────

    template <typename TMethod>
    void Register(std::function<typename InvokeMethod<TMethod>::Response(
                      NetworkPeer,
                      const typename InvokeMethod<TMethod>::Request&)> handler) {
        using M = InvokeMethod<TMethod>;
        RegisterInvokeHandler<typename M::Request, typename M::Response>(
            M::Id, std::move(handler));
    }

    template <typename TMethod>
    typename InvokeMethod<TMethod>::Response Invoke(
        const typename InvokeMethod<TMethod>::Request& request) {
        using M = InvokeMethod<TMethod>;
        return InvokeLocal<typename M::Request, typename M::Response>(M::Id, request);
    }

    template <typename TMethod>
    Future<typename InvokeMethod<TMethod>::Response> Invoke(
        NetworkPeer peer,
        const typename InvokeMethod<TMethod>::Request& request) {
        using M = InvokeMethod<TMethod>;
        return InvokeRemote<typename M::Request, typename M::Response>(peer, M::Id, request);
    }

    /**
     * Remote invoke — serializes request, sends over network, returns a Future
     * that resolves when the response arrives.
     */
    template <Serializable Request, Serializable Response>
    Future<Response> InvokeRemote(NetworkPeer peer, InvokeMethodId methodId, const Request& request) {
        uint32_t invokeId = nextInvokeId_++;

        // Serialize and send
        PacketWriter writer;
        writer.BeginPacket(PacketType::InvokeRequest, currentTick_)
              .WriteInvokeHeader(invokeId, methodId)
              .Write(request);

        auto buffer = writer.Build();
        SendPacket(peer, buffer);

        // Store a pending entry that knows how to resolve this Future
        Future<Response> future;
        PendingInvoke pending;
        pending.deserializeAndResolve = [future](SerialBuffer& buf) mutable {
            Response resp{};
            Response::Read(resp, buf);
            future.Resolve(std::move(resp));
        };
        pending_[invokeId] = std::move(pending);

        return future;
    }

private:
    void OnNetworkData(const NetworkDataMessage& msg);
    void SendPacket(NetworkPeer peer, const SerialBuffer& buffer);
    void SendInvokeResponse(NetworkPeer peer, uint32_t invokeId, InvokeMethodId methodId, const SerializableObject& response);

    uint32_t nextInvokeId_ = 1;
    uint32_t currentTick_ = 0;
    std::unordered_map<InvokeMethodId, InvokeHandler> registry_;
    std::unordered_map<uint32_t, PendingInvoke> pending_;
};

}  // namespace Elysium::Services

#define REGISTER_INVOKE(MethodType, handler) \
    Register<MethodType>( \
        std::function<typename Elysium::Services::InvokeMethod<MethodType>::Response( \
            Elysium::NetworkPeer, \
            const typename Elysium::Services::InvokeMethod<MethodType>::Request&)>( \
        [this](Elysium::NetworkPeer peer, \
               const typename Elysium::Services::InvokeMethod<MethodType>::Request& req) \
            -> typename Elysium::Services::InvokeMethod<MethodType>::Response { \
            return this->handler(peer, req); \
        }))

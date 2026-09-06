#pragma once

#include <cstdint>
#include <functional>

#include "Core/Future.h"
#include "Core/Serial.h"
#include "Network/Network.h"
#include "Interfaces/IService.h"

namespace Elysium::Services {

template <typename T>
struct InvokeMethod;

// Register<T>/Invoke<T>/RegisterInvokeHandler/InvokeLocal/InvokeRemote can't be
// virtual (templates), so the interface exposes type-erased Raw primitives and
// keeps the whole templated public API inline here, calling down to them —
// callers keep exactly the same ergonomic API the concrete InvokeService used
// to offer directly.
class IInvokeService : public IService {
   public:
    using RawHandler = std::function<SerializableObject(NetworkPeer, const SerializableObject&)>;
    using DeserializeRequestFunc = std::function<SerializableObject(SerialBuffer&)>;
    using ResolveResponseFunc = std::function<void(SerialBuffer&)>;

    virtual void RegisterRaw(InvokeMethodId methodId, RawHandler invoke,
                              DeserializeRequestFunc deserializeRequest) = 0;
    virtual SerializableObject InvokeLocalRaw(InvokeMethodId methodId, const SerializableObject& request) = 0;
    virtual void InvokeRemoteRaw(NetworkPeer peer, InvokeMethodId methodId, const SerializableObject& request,
                                  ResolveResponseFunc resolveResponse) = 0;

    template <Serializable Request, Serializable Response>
    void RegisterInvokeHandler(InvokeMethodId methodId, std::function<Response(NetworkPeer, const Request&)> handler) {
        RegisterRaw(
            methodId,
            [handler](NetworkPeer peer, const SerializableObject& reqObj) -> SerializableObject {
                SerialBuffer tmpBuf;
                reqObj.Write(tmpBuf);
                Request req{};
                Request::Read(req, tmpBuf);
                Response resp = handler(peer, req);
                return SerializableObject(std::move(resp));
            },
            [](SerialBuffer& buf) -> SerializableObject {
                Request req{};
                Request::Read(req, buf);
                return SerializableObject(std::move(req));
            });
    }

    template <Serializable Request, Serializable Response>
    Response InvokeLocal(InvokeMethodId methodId, const Request& request) {
        SerializableObject respObj = InvokeLocalRaw(methodId, SerializableObject(request));
        SerialBuffer tmpBuf;
        respObj.Write(tmpBuf);
        Response resp{};
        Response::Read(resp, tmpBuf);
        return resp;
    }

    template <Serializable Request, Serializable Response>
    Future<Response> InvokeRemote(NetworkPeer peer, InvokeMethodId methodId, const Request& request) {
        Future<Response> future;
        InvokeRemoteRaw(peer, methodId, SerializableObject(request), [future](SerialBuffer& buf) mutable {
            Response resp{};
            Response::Read(resp, buf);
            future.Resolve(std::move(resp));
        });
        return future;
    }

    // ── Type-based API (uses InvokeMethod<T> specializations) ──────────────

    template <typename TMethod>
    void Register(std::function<typename InvokeMethod<TMethod>::Response(
                       NetworkPeer, const typename InvokeMethod<TMethod>::Request&)>
                       handler) {
        using M = InvokeMethod<TMethod>;
        RegisterInvokeHandler<typename M::Request, typename M::Response>(M::Id, std::move(handler));
    }

    template <typename TMethod>
    typename InvokeMethod<TMethod>::Response Invoke(const typename InvokeMethod<TMethod>::Request& request) {
        using M = InvokeMethod<TMethod>;
        return InvokeLocal<typename M::Request, typename M::Response>(M::Id, request);
    }

    template <typename TMethod>
    Future<typename InvokeMethod<TMethod>::Response> Invoke(NetworkPeer peer,
                                                             const typename InvokeMethod<TMethod>::Request& request) {
        using M = InvokeMethod<TMethod>;
        return InvokeRemote<typename M::Request, typename M::Response>(peer, M::Id, request);
    }
};

}  // namespace Elysium::Services

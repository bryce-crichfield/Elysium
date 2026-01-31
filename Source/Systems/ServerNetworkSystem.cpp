#include "Systems/ServerNetworkSystem.h"
#include <tracy/Tracy.hpp>
#include "Core/Application.h"
#include "Services/InvokeService.h"
#include "Services/LogService.h"
#include "Services/NetworkService.h"

namespace Elysium {

ServerNetworkSystem::ServerNetworkSystem(Context context)
    : System(context) {
    auto& invoke = Application::GetInstance().GetService<Services::InvokeService>();
    invoke.Register<Ping>(
        std::function<PingResponse(NetworkPeer, const PingRequest&)>(
        [this](NetworkPeer peer, const PingRequest& req) -> PingResponse {
            LOG_INFOF("ServerNet", "Received Ping from peer, clientTick=%u", req.clientTick);
            PingResponse resp;
            resp.serverTick = currentTick_;
            resp.echoClientTick = req.clientTick;
            return resp;
        }));
    LOG_INFO("ServerNet", "Registered Ping handler");
}

void ServerNetworkSystem::Update(float deltaTime) {
    ZoneScopedN("ServerNetworkSystem::Update");
}

void ServerNetworkSystem::OnMessage(Message& message) {
    ZoneScopedN("ServerNetworkSystem::OnMessage");
}

}  // namespace Elysium

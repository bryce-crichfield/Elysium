#include "ClientNetworkSystem.h"
#include <tracy/Tracy.hpp>
#include "Core/Application.h"
#include "Services/InvokeService.h"
#include "Services/LogService.h"
#include "Services/NetworkService.h"

namespace Elysium {
using namespace Generated;

ClientNetworkSystem::ClientNetworkSystem(Context context)
    : System(context) {
    // System is created after connection is already established, so fire ping immediately
    OnConnectedToServer();
}

void ClientNetworkSystem::Update(float deltaTime) {
    ZoneScopedN("ClientNetworkSystem::Update");

    if (waitingForPing_ && pingFuture_.IsReady()) {
        auto resp = pingFuture_.Get();
        LOG_INFOF("ClientNet", "Ping response: serverTick=%u, echoClientTick=%u",
                  resp.serverTick, resp.echoClientTick);
        waitingForPing_ = false;
    }
}

void ClientNetworkSystem::OnEvent(Event& event) {
    ZoneScopedN("ClientNetworkSystem::OnEvent");
}

void ClientNetworkSystem::OnMessage(Message& message) {
    ZoneScopedN("ClientNetworkSystem::OnMessage");
}

void ClientNetworkSystem::OnConnectedToServer() {
    LOG_INFO("ClientNet", "Connected to server — sending test Ping RPC");

    auto& invoke = Application::GetInstance().GetService<Services::InvokeService>();

    PingRequest req;
    req.clientTick = lastServerTick_;

    auto future = invoke.Invoke<Ping>(SERVER_PEER, req);
    pingFuture_ = future;
    waitingForPing_ = true;
}

}  // namespace Elysium

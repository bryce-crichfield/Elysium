#include "Systems/ServerNetworkSystem.h"
#include "Network/Generated.h"
#include <tracy/Tracy.hpp>
#include "Core/Application.h"
#include "Services/InvokeService.h"
#include "Services/LogService.h"
#include "Services/NetworkService.h"

namespace Elysium {
using namespace Generated;

ServerNetworkSystem::ServerNetworkSystem(Context context)
    : System(context) {
}

void ServerNetworkSystem::Update(float deltaTime) {
    ZoneScopedN("ServerNetworkSystem::Update");
}

void ServerNetworkSystem::OnMessage(Message& message) {
    ZoneScopedN("ServerNetworkSystem::OnMessage");
}

}  // namespace Elysium

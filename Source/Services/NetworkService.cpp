#include "Services/NetworkService.h"
#include "Common.h"

namespace Elysium::Services {

NetworkService::NetworkService() {
    name_ = "NetworkService";
}

void NetworkService::Initialize() {
    isServer_ = false;
    isConnected_ = false;
    isVisible_ = false;
}

void NetworkService::Shutdown() {
    if (isConnected_) {
        Disconnect();
    }
}

void NetworkService::Update(float deltaTime) {
    Profile;
}

void NetworkService::OnDebugDraw() {
    Profile;
    // No debug UI for NetworkService currently
}

bool NetworkService::StartServer(int port) {
    return false;
}

bool NetworkService::ConnectToServer(const std::string& address, int port) {
    return false;
}

void NetworkService::Disconnect() {
    isConnected_ = false;
    isServer_ = false;
}

void NetworkService::SendData(const void* data, size_t size) {
}

bool NetworkService::IsConnected() const {
    return isConnected_;
}

} // namespace Elysium::Services

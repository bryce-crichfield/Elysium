#pragma once

#include "Service.h"
#include <string>

namespace Elysium::Services {

class NetworkService : public Elysium::Service {
public:
    NetworkService();
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void OnDebugDraw() override;
    
    bool StartServer(int port);
    bool ConnectToServer(const std::string& address, int port);
    void Disconnect();
    
    void SendData(const void* data, size_t size);
    bool IsConnected() const;

private:
    bool isServer_ = false;
    bool isConnected_ = false;
};

} // namespace Elysium::Services
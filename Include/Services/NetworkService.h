#pragma once

#include <string>

namespace Elysium::Services {

class NetworkService {
public:
    void Initialize();
    void Shutdown();
    void Update();
    
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
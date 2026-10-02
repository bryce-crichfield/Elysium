#pragma once

#include <string>
#include "Editor/Editor.h"
#include "Core/Future.h"
#include "Network/Network.h"
#include "Network/Generated.h"

namespace Elysium::Services {
class INetworkService;
}

namespace Elysium {

class NetworkEditor : public Editor {
   public:
    static constexpr const char* Title = "Network";

    explicit NetworkEditor(ServiceLocator& services);

    void Draw() override;
    // Opened from the View menu as a modal dialog rather than living in the dock layout.
    bool IsDocked() const override { return false; }

   private:
    void DrawStatus(Services::INetworkService& service);
    void DrawConnect(Services::INetworkService& service);
    void DrawStats(Services::INetworkService& service);
    void DrawPing();

    char addressBuffer_[128] = "127.0.0.1";
    int port_ = 7777;

    bool waitingForPing_ = false;
    bool hasPingResult_ = false;
    Future<Generated::PingResponse> pingFuture_;
    uint32_t pingCounter_ = 0;
    Generated::PingResponse lastPingResponse_;
};

}  // namespace Elysium

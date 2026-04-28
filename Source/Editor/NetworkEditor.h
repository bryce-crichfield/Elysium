#pragma once

#include <string>
#include "Core/Editor.h"
#include "Core/Future.h"
#include "Network/Network.h"
#include "Network/Generated.h"

namespace Elysium::Services {
class NetworkService;
}

namespace Elysium {

class NetworkEditor : public Editor {
   public:
    NetworkEditor();

    void Draw(Application& app) override;

   private:
    char addressBuffer_[128] = "127.0.0.1";
    int port_ = 7777;

    bool waitingForPing_ = false;
    bool hasPingResult_ = false;
    Future<Generated::PingResponse> pingFuture_;
    uint32_t pingCounter_ = 0;
    Generated::PingResponse lastPingResponse_;
};

}  // namespace Elysium

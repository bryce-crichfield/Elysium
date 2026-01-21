#pragma once

#include <string>
#include "Core/Editor.h"

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
};

}  // namespace Elysium

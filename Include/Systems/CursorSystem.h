#pragma once

#include "System.h"

namespace Elysium::Systems {
class CursorSystem : public System {
   public:
    CursorSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;
};
};  // namespace Elysium::Systems

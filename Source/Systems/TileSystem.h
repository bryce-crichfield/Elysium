#pragma once

#include "Core/System.h"

namespace Elysium::Systems {

class TileSystem : public System {
   public:
    TileSystem(Context context);
    void Update(float deltaTime) override;

   private:
    bool initialized_ = false;
};

}  // namespace Elysium::Systems

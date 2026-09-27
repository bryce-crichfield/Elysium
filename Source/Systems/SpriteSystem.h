#pragma once

#include "Core/System.h"

namespace Elysium::Systems {

class SpriteSystem : public System {
   public:
    SpriteSystem(Context context);
    void Update(float deltaTime) override;
    bool RunsWhenPaused() const override { return true; }
};

}  // namespace Elysium::Systems

#pragma once

#include "System.h"

namespace Elysium::Systems {

class SpriteSystem : public System {
public:
    SpriteSystem(Context context);
    void Update(float deltaTime) override;
};

} // namespace Elysium::Systems

#pragma once

#include "System.h"

namespace Elysium::Systems {

class RenderSystem : public System {
public:
    RenderSystem(Context context) : System(context) {}

    void Update(float deltaTime) override {}
    void Render() override;
};

} // namespace Elysium::Systems

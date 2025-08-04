#pragma once

#include "Entity.h"

namespace Elysium::Systems {

class RenderSystem : public System {
public:
    RenderSystem(EntityWorld* world) : System(world) {}
    
    void Update(float deltaTime) override {}
    void Render() override;
};

} // namespace Elysium::Systems
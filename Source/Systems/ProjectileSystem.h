#pragma once

#include "Core/System.h"

namespace Elysium::Systems {

class ProjectileSystem : public System {
public:
    ProjectileSystem(Context context);
    void Update(float deltaTime) override;
};

} // namespace Elysium::Systems

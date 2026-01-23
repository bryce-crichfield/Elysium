#pragma once

#include "Core/System.h"

namespace Elysium::Systems {

class SpatialSystem; // Forward declare

class PathfindingSystem : public System {
public:
    PathfindingSystem(Context context);
    void Update(float deltaTime) override;

private:
    SpatialSystem* spatialSystem_ = nullptr;
};

} // namespace Elysium::Systems

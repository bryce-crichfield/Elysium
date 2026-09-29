#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include <queue>

namespace Elysium::Systems {

class SpatialSystem;
class NavMeshSystem;

struct MoveCommand {
    Entity entity;
    Vector2 target;
};

class MovementSystem : public System {
   private:
    std::queue<MoveCommand> moveCommands_;       // System will route entities towards these targets using pathfinding  
    SpatialSystem* spatialSystem_ = nullptr;   // legacy tilemap grid
    NavMeshSystem* navMesh_ = nullptr;         // preferred when the scene has one

   public:
    MovementSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;

    void IssueMoveCommand(Entity entity, Vector2 target) {
        moveCommands_.push({entity, target});
    }
};
}  // namespace Elysium::Systems

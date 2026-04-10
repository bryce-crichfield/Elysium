#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include "raylib.h"
#include <queue>

namespace Elysium::Systems {

class SpatialSystem;

struct MoveCommand {
    Entity entity;
    Vector2 target;
};

class MovementSystem : public System {
   private:
    std::queue<MoveCommand> moveCommands_;       // System will route entities towards these targets using pathfinding  
    SpatialSystem* spatialSystem_ = nullptr;

   public:
    MovementSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;

    void IssueMoveCommand(Entity entity, Vector2 target) {
        moveCommands_.push({entity, target});
    }
};
}  // namespace Elysium::Systems

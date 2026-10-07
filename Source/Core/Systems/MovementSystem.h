#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include "Core/Math/MathTypes.h"
#include <queue>

namespace Elysium::Systems {

class NavigationSystem;

struct MoveCommand {
    Entity entity;
    Vector2 target;
};

class MovementSystem : public System {
   private:
    std::queue<MoveCommand> moveCommands_;       // System will route entities towards these targets using pathfinding  
    NavigationSystem* navMesh_ = nullptr;         // preferred when the scene has one

   public:
    MovementSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;

    void IssueMoveCommand(Entity entity, Vector2 target) {
        moveCommands_.push({entity, target});
    }
};
}  // namespace Elysium::Systems

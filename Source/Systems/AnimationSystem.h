#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "Entity.h"
#include "System.h"
#include "raylib.h"

namespace Elysium::Systems {

class AnimationSystem : public System {
   public:
    AnimationSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;
};
}  // namespace Elysium::Systems

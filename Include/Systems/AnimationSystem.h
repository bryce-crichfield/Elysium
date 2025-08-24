#pragma once

#include "System.h"
#include "raylib.h"
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include "Entity.h"

namespace Elysium::Systems {

class AnimationSystem : public System
{
public:
    AnimationSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;
};
}

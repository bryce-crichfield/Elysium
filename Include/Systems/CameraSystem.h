#pragma once

#include "System.h"
#include "raylib.h"

namespace Elysium::Systems {

class CameraSystem : public System {
public:
    CameraSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;

private:
    // Helper function to lerp between two Vector2 values
    Vector2 LerpVector2(Vector2 start, Vector2 end, float t);
};

} // namespace Elysium::Systems

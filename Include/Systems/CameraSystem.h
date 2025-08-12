#pragma once

#include "System.h"
#include "raylib.h"

namespace Elysium::Systems {

class CameraSystem : public System {
public:
    CameraSystem(Context context, float lerpSpeed = 5.0f) : System(context), lerpSpeed_(lerpSpeed) {}

    void Update(float deltaTime) override;

    float GetLerpSpeed() const { return lerpSpeed_; }
    void SetLerpSpeed(float speed) { lerpSpeed_ = speed; }
private:
    float lerpSpeed_;

    // Helper function to lerp between two Vector2 values
    Vector2 LerpVector2(Vector2 start, Vector2 end, float t);
};

} // namespace Elysium::Systems

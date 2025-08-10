#pragma once

#include "System.h"
#include "raylib.h"

namespace Elysium::Systems {

class CameraSystem : public System {
public:
    CameraSystem(Context context, float lerpSpeed = 5.0f) : System(context), lerpSpeed_(lerpSpeed), activeCamera_(nullptr) {}

    void Update(float deltaTime) override;
    void SetLerpSpeed(float speed) { lerpSpeed_ = speed; }
    float GetLerpSpeed() const { return lerpSpeed_; }

    // Camera integration methods
    void BeginCameraMode();
    void EndCameraMode();
    bool HasActiveCamera() const { return activeCamera_ != nullptr; }
    Camera2D* GetActiveCamera() const { return activeCamera_; }

private:
    float lerpSpeed_;
    Camera2D* activeCamera_;

    // Helper function to lerp between two Vector2 values
    Vector2 LerpVector2(Vector2 start, Vector2 end, float t);
};

} // namespace Elysium::Systems

#pragma once

#include "raylib.h"

namespace Elysium {

class Scene;

namespace Services {
    enum class SceneState;
}

class SceneRenderer {
public:
    SceneRenderer() = default;
    ~SceneRenderer() = default;

    void Initialize(int framebufferWidth, int framebufferHeight);
    void Shutdown();

    // Main render call - renders the scene with optional transition effects
    void Render(Scene* scene, Services::SceneState state, float transitionProgress, bool isTransitioning, Color backgroundColor);

    // Called when window is resized to recalculate letterboxing
    void OnWindowResized();

    // Getters for debug/inspection
    const Rectangle& GetLetterboxRect() const { return letterboxRect_; }
    float GetScaleX() const { return scaleX_; }
    float GetScaleY() const { return scaleY_; }

private:
    void CalculateLetterboxing();

    RenderTexture2D framebuffer_;
    Rectangle letterboxRect_;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    Vector2 offset_ = {0, 0};
};

} // namespace Elysium

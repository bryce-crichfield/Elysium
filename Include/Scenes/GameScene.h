#pragma once

#include "../Scene.h"
#include "Services/MemoryTracker.h"
#include "raylib.h"
#include <vector>

namespace Elysium::Scenes {

struct Ball {
    Vector2 position;
    Vector2 velocity;
    float radius;
    Color color;
};

class GameScene : public Scene {
public:
    GameScene(const GameConfig& config);
    virtual ~GameScene() = default;
    
    void OnUpdate(float deltaTime) override;
    void OnDraw() override;
    void OnDebugDraw() override;
    void OnInput(const InputEvent& event) override;
    void OnEnter() override;
    void OnExit() override;

private:
    void AddBall();
    void ClearBalls();
    
    // Use tracked allocator for demonstrating memory tracking
    std::vector<Ball, Elysium::Services::TrackedAllocator<Ball>> balls_;
    float gravity_;
    bool paused_;
    int ballCount_;
};

} // namespace Elysium::Scenes
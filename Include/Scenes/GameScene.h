#pragma once

#include "../Scene.h"
#include "Services/MemoryTracker.h"
#include "raylib.h"

namespace Elysium::Scenes {

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

protected:
    void CreateCustomSystems() override;
    
private:
    void AddBall();
    void AddBallAtPosition(float x, float y);
    void ClearBalls();
    
    bool paused_;
};

} // namespace Elysium::Scenes
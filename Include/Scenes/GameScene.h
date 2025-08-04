#pragma once

#include "../Scene.h"
#include "Services/MemoryTracker.h"
#include "Entity.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "raylib.h"
#include <vector>
#include <memory>

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

private:
    void AddBall();
    void AddBallAtPosition(float x, float y);
    void ClearBalls();
    
    std::unique_ptr<EntityWorld> world_;
    std::unique_ptr<Elysium::Systems::PhysicsSystem> physicsSystem_;
    std::unique_ptr<Elysium::Systems::RenderSystem> renderSystem_;
    
    bool paused_;
};

} // namespace Elysium::Scenes
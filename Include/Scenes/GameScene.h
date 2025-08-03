#pragma once

#include "../Scene.h"
#include "Services/MemoryTracker.h"
#include "Entity.h"
#include "raylib.h"
#include <vector>
#include <memory>

namespace Elysium::Scenes {

class PhysicsSystem : public System {
public:
    PhysicsSystem(EntityWorld* world, float gravity = 500.0f) : System(world), gravity_(gravity) {}
    
    void Update(float deltaTime) override;
    void SetGravity(float gravity) { gravity_ = gravity; }
    float GetGravity() const { return gravity_; }
    
private:
    float gravity_;
};

class RenderSystem : public System {
public:
    RenderSystem(EntityWorld* world) : System(world) {}
    
    void Update(float deltaTime) override {}
    void Render() override;
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
    void AddBallAtPosition(float x, float y);
    void ClearBalls();
    
    std::unique_ptr<EntityWorld> world_;
    std::unique_ptr<PhysicsSystem> physicsSystem_;
    std::unique_ptr<RenderSystem> renderSystem_;
    
    bool paused_;
};

} // namespace Elysium::Scenes
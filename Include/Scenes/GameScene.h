#pragma once

#include "../Scene.h"
#include "Services/MemoryTracker.h"
#include "raylib.h"

namespace Elysium::Scenes {

class GameScene : public Scene {
public:
    GameScene();
    virtual ~GameScene() = default;

    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnDebugDraw() override;
    void OnEnter() override;
    void OnExit() override;

protected:
    void CreateCustomSystems() override;

private:
    void CreatePlayer();
    void CreateHouse();
    void DrawGrid(Rectangle screen);
    void HandlePlayerMovement(float deltaTime);
    void MovePlayerWithAnimation(float deltaX, float deltaY);
    Vector2 GetLastTargetPosition();

    Entity player_;
    Entity house_;
    Vector2 lastTargetPos_; // Track the target of the last queued movement
    static constexpr float GRID_SIZE = 32.0f;
};

} // namespace Elysium::Scenes

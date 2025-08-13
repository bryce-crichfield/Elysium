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
};

} // namespace Elysium::Scenes

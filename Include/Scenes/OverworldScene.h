#pragma once

#include "../Scene.h"
#include "raylib.h"

namespace Elysium::Scenes {

class OverworldScene : public Scene {
public:
    OverworldScene();
    virtual ~OverworldScene() = default;

    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnDebugDraw() override;
    void OnEnter() override;
    void OnExit() override;
};

} // namespace Elysium::Scenes

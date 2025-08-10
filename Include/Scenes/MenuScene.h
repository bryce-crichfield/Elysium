#pragma once

#include "../Scene.h"
#include "raylib.h"

namespace Elysium::Scenes {

class MenuScene : public Scene {
public:
    MenuScene();
    virtual ~MenuScene() = default;

    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnDebugDraw() override;
    void OnEnter() override;
    void OnExit() override;

private:
    float rotation_;
    Color backgroundColor_;
};

} // namespace Elysium::Scenes

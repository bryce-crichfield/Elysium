#pragma once

#include "Event.h"
#include "Scene.h"
#include "raylib.h"

namespace Elysium::Scenes {

class MenuScene : public Scene {
   public:
    MenuScene();
    virtual ~MenuScene() = default;

    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnEnter() override;
    void OnExit() override;
    void OnEvent(Event& event) override;

   private:
    float rotation_;
    Color backgroundColor_;
};

}  // namespace Elysium::Scenes

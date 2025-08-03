#pragma once

#include "Scene.h"
#include "raylib.h"

class MenuScene : public Scene {
public:
    MenuScene();
    virtual ~MenuScene() = default;
    
    void OnUpdate(float deltaTime) override;
    void OnDraw() override;
    void OnInput(const InputEvent& event) override;
    void OnEnter() override;
    void OnExit() override;

private:
    float rotation_;
    Color backgroundColor_;
};
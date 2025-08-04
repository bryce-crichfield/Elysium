#pragma once

#include "raylib.h"
#include "Scene.h"

namespace Elysium::Scenes {

class BattleScene : public Scene {
public:
    BattleScene(const GameConfig& config);
    virtual ~BattleScene() = default;
    void OnUpdate(float deltaTime) override;
    void OnDraw() override;
    void OnDebugDraw() override;
    void OnInput(const InputEvent& event) override;
    void OnEnter() override;
    void OnExit() override;
};
};
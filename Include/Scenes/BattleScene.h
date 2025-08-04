#pragma once

#include "raylib.h"
#include "Scene.h"
#include "Entity.h"
#include <memory>
#include <vector>

namespace Elysium::Scenes {

void LoadBattleScene(std::string xmlPath, EntityWorld* world, std::vector<std::unique_ptr<System>>* systems);

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
private:
    std::unique_ptr<EntityWorld> world;
    std::vector<std::unique_ptr<System>> systems;
};
};
#pragma once

#include "raylib.h"
#include "Scene.h"
#include "Systems/AnimationSystem.h"
#include "Systems/BattleSystem.h"
#include "Systems/TurnSystem.h"
#include <memory>

namespace Elysium {
    struct Action;
    enum class BattleState;
}

namespace Elysium::Events {
    struct BattleEndEvent;
}

namespace Elysium::Scenes {

class BattleScene : public Scene {
public:
    BattleScene();
    virtual ~BattleScene() = default;

    void OnEnter() override;
    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnExit() override;

    std::vector<Asset> GetAssets() override;

    void StartBattle(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits);
    Systems::BattleState GetBattleState() const;

private:
    void OnBattleEnd(const Events::BattleEndEvent& event);
    void SetupBattleEntities();

    // System references
    Systems::BattleSystem* battleSystem = nullptr;
    TurnSystem* turnSystem = nullptr;

    // UI state
    int selectedUnit = -1;
};
}

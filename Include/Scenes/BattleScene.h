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
    void OnDebugDraw() override;
    void OnExit() override;

    std::vector<Asset> GetAssets() override;

    void StartBattle(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits);
    BattleState GetBattleState() const;

private:
    void InitializeEventHandlers();
    void OnBattleEnd(const Events::BattleEndEvent& event);

    std::unique_ptr<BattleSystem> battleSystem;
    std::unique_ptr<TurnSystem> turnSystem;
};
}

#pragma once

#include "raylib.h"
#include "Scene.h"
#include "Systems/AnimationSystem.h"
#include <memory>

namespace Elysium { struct Action; }

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
private:
    // Action factory methods for different behavior types
    std::shared_ptr<Elysium::Action> CreateHeroIdleAnimation();
    std::shared_ptr<Elysium::Action> CreateEnemyBreathingAnimation(Entity enemy);
    std::shared_ptr<Elysium::Action> CreateHeroAttackSequence(Entity hero);
    std::shared_ptr<Elysium::Action> CreateHeroDefensiveStance(Entity hero);
    std::shared_ptr<Elysium::Action> CreateEnemyChargeAttack(Entity enemy);
    std::shared_ptr<Elysium::Action> CreateEnemyMagicCast(Entity enemy);
    std::shared_ptr<Elysium::Action> CreateHeroCombatSequence(Entity hero);
    std::shared_ptr<Elysium::Action> CreateEnemyCombatSequence(Entity enemy);
};
};

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
    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnDebugDraw() override;
    void OnInput(const InputEvent& event) override;
    void OnEnter() override;
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
    
    // Asset storage
    Texture2D warriorTexture_;
    Texture2D swordTexture_;
    Texture2D shieldTexture_;
    Texture2D battlegroundTexture_;
    Sound battleMusic_;
    Sound swordClash_;
    Sound victoryFanfare_;
    
    bool assetsLoaded_{false};
};
};
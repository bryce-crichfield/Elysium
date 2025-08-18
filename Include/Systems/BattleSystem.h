#pragma once

#include "System.h"
#include "Events/BattleEvents.h"
#include <vector>
#include <string>

namespace Elysium::Systems {
    enum class BattleState {
        Initializing,
        PlayerTurn,
        EnemyTurn,
        Victory,
        Defeat,
        Ended
    };

    class BattleSystem : public System {
    public:
        BattleSystem(Context context);
        ~BattleSystem();

        void Update(float deltaTime) override;
        void StartBattle(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits);
        void EndBattle(bool playerVictory);

        BattleState GetCurrentState() const { return currentState; }
        bool IsBattleActive() const { return currentState != BattleState::Ended; }
    private:
        void OnAttackEvent(const Events::AttackEvent& event);
        void OnUnitKilled(const Events::UnitKilledEvent& event);
        void OnTurnEnd(const Events::TurnEndEvent& event);

        BattleState currentState;
        std::string currentBattleId;
        float battleTimer;
    };
}

#pragma once

#include "Events/BattleEvents.h"
#include <vector>
#include <string>

namespace Elysium {

    enum class BattleState {
        Initializing,
        PlayerTurn,
        EnemyTurn,
        Victory,
        Defeat,
        Ended
    };

    class BattleSystem {
    public:
        BattleSystem();
        ~BattleSystem();

        void Update(float deltaTime);
        void StartBattle(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits);
        void EndBattle(bool playerVictory);

        BattleState GetCurrentState() const { return currentState; }
        bool IsBattleActive() const { return currentState != BattleState::Ended; }

    private:
        void OnAttackEvent(const Events::AttackEvent& event);
        void OnUnitDefeated(const Events::UnitDefeatedEvent& event);
        void OnTurnEnd(const Events::TurnEndEvent& event);

        void CheckBattleEndConditions();
        void ProcessBattleLogic();
        void InitializeEventHandlers();

        BattleState currentState;
        std::string currentBattleId;
        std::vector<int> playerUnits;
        std::vector<int> enemyUnits;
        std::vector<int> activePlayerUnits;
        std::vector<int> activeEnemyUnits;
        float battleTimer;
    };
}

#pragma once

#include "System.h"
#include "Events/BattleEvents.h"
#include <vector>

namespace Elysium {

    struct TurnOrder {
        int unitId;
        int initiative;
        bool isPlayerUnit;
    };

    class TurnSystem : public System {
    public:
        TurnSystem(Context context);
        ~TurnSystem();

        void Update(float deltaTime) override;
        void InitializeTurnOrder(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits);
        void NextTurn();
        void EndCurrentTurn();

        int GetCurrentUnit() const { return currentUnit; }
        int GetTurnNumber() const { return turnNumber; }
        bool IsPlayerTurn() const { return isCurrentPlayerTurn; }
        bool HasActiveTurn() const { return currentUnit != -1; }

    private:
        void OnBattleStart(const Events::BattleStartEvent& event);
        void OnUnitDefeated(const Events::UnitKilledEvent& event);
        void OnActionSelected(const Events::ActionSelectedEvent& event);

        void CalculateInitiative();
        void StartTurn();
        void RemoveUnitFromTurnOrder(int unitId);
        int CalculateInitiativeForUnit(int unitId);

        std::vector<TurnOrder> turnQueue;
        int currentTurnIndex;
        int currentUnit;
        int turnNumber;
        bool isCurrentPlayerTurn;
        float turnTimer;
        const float maxTurnTime = 30.0f;
    };
}

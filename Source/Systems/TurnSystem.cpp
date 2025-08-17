#include "Systems/TurnSystem.h"
#include "Application.h"
#include "Services/LogService.h"
#include <algorithm>
#include <random>

namespace Elysium {

TurnSystem::TurnSystem()
    : currentTurnIndex(-1)
    , currentUnit(-1)
    , turnNumber(1)
    , isCurrentPlayerTurn(false)
    , turnTimer(0.0f) {
    InitializeEventHandlers();
}

TurnSystem::~TurnSystem() {
}

void TurnSystem::InitializeEventHandlers() {
    auto& eventService = Application::GetInstance().GetEventService();

    eventService.Subscribe<Events::BattleStartEvent>([this](const Events::BattleStartEvent& event) {
        OnBattleStart(event);
    });

    eventService.Subscribe<Events::UnitDefeatedEvent>([this](const Events::UnitDefeatedEvent& event) {
        OnUnitDefeated(event);
    });

    eventService.Subscribe<Events::ActionSelectedEvent>([this](const Events::ActionSelectedEvent& event) {
        OnActionSelected(event);
    });
}

void TurnSystem::Update(float deltaTime) {
    if (!HasActiveTurn()) return;

    turnTimer += deltaTime;

    if (turnTimer >= maxTurnTime) {
        LOG_WARNING("TurnSystem", "Turn time exceeded for unit " + std::to_string(currentUnit));
        EndCurrentTurn();
    }
}

void TurnSystem::InitializeTurnOrder(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits) {
    turnQueue.clear();

    for (int unitId : playerUnits) {
        TurnOrder order;
        order.unitId = unitId;
        order.initiative = CalculateInitiativeForUnit(unitId);
        order.isPlayerUnit = true;
        turnQueue.push_back(order);
    }

    for (int unitId : enemyUnits) {
        TurnOrder order;
        order.unitId = unitId;
        order.initiative = CalculateInitiativeForUnit(unitId);
        order.isPlayerUnit = false;
        turnQueue.push_back(order);
    }

    CalculateInitiative();

    currentTurnIndex = 0;
    turnNumber = 1;
    StartTurn();

    LOG_INFO("TurnSystem", "Turn order initialized with " + std::to_string(turnQueue.size()) + " units");
}

void TurnSystem::NextTurn() {
    if (turnQueue.empty()) return;

    currentTurnIndex = (currentTurnIndex + 1) % turnQueue.size();

    if (currentTurnIndex == 0) {
        turnNumber++;
        LOG_INFO("TurnSystem", "Starting turn " + std::to_string(turnNumber));
    }

    StartTurn();
}

void TurnSystem::EndCurrentTurn() {
    if (!HasActiveTurn()) return;

    auto& eventService = Application::GetInstance().GetEventService();
    Events::TurnEndEvent endEvent;
    endEvent.unitId = currentUnit;
    endEvent.turnNumber = turnNumber;
    eventService.FireEvent(endEvent);

    LOG_INFO("TurnSystem", "Ending turn for unit " + std::to_string(currentUnit));

    NextTurn();
}

void TurnSystem::OnBattleStart(const Events::BattleStartEvent& event) {
    InitializeTurnOrder(event.playerUnits, event.enemyUnits);
}

void TurnSystem::OnUnitDefeated(const Events::UnitDefeatedEvent& event) {
    RemoveUnitFromTurnOrder(event.unitId);
}

void TurnSystem::OnActionSelected(const Events::ActionSelectedEvent& event) {
    if (event.unitId == currentUnit) {
        EndCurrentTurn();
    }
}

void TurnSystem::CalculateInitiative() {
    std::sort(turnQueue.begin(), turnQueue.end(), [](const TurnOrder& a, const TurnOrder& b) {
        return a.initiative > b.initiative;
    });
}

void TurnSystem::StartTurn() {
    if (turnQueue.empty() || currentTurnIndex >= static_cast<int>(turnQueue.size())) {
        currentUnit = -1;
        isCurrentPlayerTurn = false;
        return;
    }

    const auto& currentTurn = turnQueue[currentTurnIndex];
    currentUnit = currentTurn.unitId;
    isCurrentPlayerTurn = currentTurn.isPlayerUnit;
    turnTimer = 0.0f;

    auto& eventService = Application::GetInstance().GetEventService();
    Events::TurnStartEvent startEvent;
    startEvent.unitId = currentUnit;
    startEvent.turnNumber = turnNumber;
    eventService.FireEvent(startEvent);

    LOG_INFO("TurnSystem", "Starting turn for " + std::string(isCurrentPlayerTurn ? "player" : "enemy") + " unit " + std::to_string(currentUnit));
}

void TurnSystem::RemoveUnitFromTurnOrder(int unitId) {
    auto it = std::find_if(turnQueue.begin(), turnQueue.end(), [unitId](const TurnOrder& order) {
        return order.unitId == unitId;
    });

    if (it != turnQueue.end()) {
        int removedIndex = std::distance(turnQueue.begin(), it);
        turnQueue.erase(it);

        if (currentTurnIndex > removedIndex) {
            currentTurnIndex--;
        } else if (currentTurnIndex >= static_cast<int>(turnQueue.size())) {
            currentTurnIndex = 0;
        }

        if (currentUnit == unitId) {
            NextTurn();
        }

        LOG_INFO("TurnSystem", "Removed unit " + std::to_string(unitId) + " from turn order");
    }
}

int TurnSystem::CalculateInitiativeForUnit(int unitId) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 20);
    return dis(gen) + 10;
}

} // namespace Elysium

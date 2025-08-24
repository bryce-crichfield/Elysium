#include "Application.h"
#include "Systems/TurnSystem.h"
#include "Services/LogService.h"
#include "Services/EventService.h"
#include <algorithm>
#include <random>

namespace Elysium {

TurnSystem::TurnSystem(Context context)
    : System(context)
    , currentTurnIndex(-1)
    , currentUnit(-1)
    , turnNumber(1)
    , isCurrentPlayerTurn(false)
    , turnTimer(0.0f) {
    auto& eventService = Application::GetInstance().GetService<Elysium::Services::EventService>("EventService");

    eventService.Subscribe<Events::BattleStartEvent>([this](const Events::BattleStartEvent& event) {
        OnBattleStart(event);
    });

    eventService.Subscribe<Events::UnitKilledEvent>([this](const Events::UnitKilledEvent& event) {
        OnUnitDefeated(event);
    });

    eventService.Subscribe<Events::ActionSelectedEvent>([this](const Events::ActionSelectedEvent& event) {
        OnActionSelected(event);
    });
}

TurnSystem::~TurnSystem() {
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

    CalculateInitiative();

    currentTurnIndex = 0;
    turnNumber = 1;
    StartTurn();

    LOG_DEBUG("TurnSystem", "Turn order initialized with " + std::to_string(turnQueue.size()) + " units");
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

    auto& eventService = Application::GetInstance().GetService<Elysium::Services::EventService>("EventService");
    Events::TurnEndEvent endEvent;
    endEvent.teamId = isCurrentPlayerTurn ? 0 : 1;
    endEvent.turnNumber = turnNumber;
    eventService.FireEvent(endEvent);

    LOG_INFO("TurnSystem", "Ending turn for unit " + std::to_string(currentUnit));

    NextTurn();
}

void TurnSystem::OnBattleStart(const Events::BattleStartEvent& event) {
    std::vector<int> playerUnits;  // Empty for now
    std::vector<int> enemyUnits;   // Empty for now
    InitializeTurnOrder(playerUnits, enemyUnits);
}

void TurnSystem::OnUnitDefeated(const Events::UnitKilledEvent& event) {
    RemoveUnitFromTurnOrder(event.unit);
}

void TurnSystem::OnActionSelected(const Events::ActionSelectedEvent& event) {
    if (event.unit == currentUnit) {
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

    auto& eventService = Application::GetInstance().GetService<Elysium::Services::EventService>("EventService");
    Events::TurnStartEvent startEvent;
    startEvent.teamId = isCurrentPlayerTurn ? 0 : 1;
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

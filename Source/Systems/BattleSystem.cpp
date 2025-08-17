#include "Systems/BattleSystem.h"
#include "Application.h"
#include "Services/LogService.h"
#include <algorithm>

namespace Elysium {

BattleSystem::BattleSystem()
    : currentState(BattleState::Ended)
    , battleTimer(0.0f) {
    InitializeEventHandlers();
}

BattleSystem::~BattleSystem() {
}

void BattleSystem::InitializeEventHandlers() {
    auto& eventService = Application::GetInstance().GetEventService();

    eventService.Subscribe<Events::AttackEvent>([this](const Events::AttackEvent& event) {
        OnAttackEvent(event);
    });

    eventService.Subscribe<Events::UnitDefeatedEvent>([this](const Events::UnitDefeatedEvent& event) {
        OnUnitDefeated(event);
    });

    eventService.Subscribe<Events::TurnEndEvent>([this](const Events::TurnEndEvent& event) {
        OnTurnEnd(event);
    });
}

void BattleSystem::Update(float deltaTime) {
    if (!IsBattleActive()) return;

    battleTimer += deltaTime;
    ProcessBattleLogic();
}

void BattleSystem::StartBattle(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits) {
    currentState = BattleState::Initializing;
    this->playerUnits = playerUnits;
    this->enemyUnits = enemyUnits;
    this->activePlayerUnits = playerUnits;
    this->activeEnemyUnits = enemyUnits;
    battleTimer = 0.0f;
    currentBattleId = "battle_" + std::to_string(battleTimer);

    auto& eventService = Application::GetInstance().GetEventService();
    Events::BattleStartEvent startEvent;
    startEvent.battleId = currentBattleId;
    startEvent.playerUnits = playerUnits;
    startEvent.enemyUnits = enemyUnits;
    eventService.FireEvent(startEvent);

    currentState = BattleState::PlayerTurn;
    LOG_INFO("BattleSystem", "Battle started with " + std::to_string(playerUnits.size()) + " player units vs " + std::to_string(enemyUnits.size()) + " enemy units");
}

void BattleSystem::EndBattle(bool playerVictory) {
    if (currentState == BattleState::Ended) return;

    currentState = BattleState::Ended;

    auto& eventService = Application::GetInstance().GetEventService();
    Events::BattleEndEvent endEvent;
    endEvent.battleId = currentBattleId;
    endEvent.playerVictory = playerVictory;
    eventService.FireEvent(endEvent);

    LOG_INFO("BattleSystem", "Battle ended - " + std::string(playerVictory ? "Player Victory" : "Player Defeat"));
}

void BattleSystem::OnAttackEvent(const Events::AttackEvent& event) {
    LOG_INFO("BattleSystem", "Processing attack from unit " + std::to_string(event.attackerId) + " to unit " + std::to_string(event.targetId) + " for " + std::to_string(event.damage) + " damage");
}

void BattleSystem::OnUnitDefeated(const Events::UnitDefeatedEvent& event) {
    auto playerIt = std::find(activePlayerUnits.begin(), activePlayerUnits.end(), event.unitId);
    auto enemyIt = std::find(activeEnemyUnits.begin(), activeEnemyUnits.end(), event.unitId);

    if (playerIt != activePlayerUnits.end()) {
        activePlayerUnits.erase(playerIt);
        LOG_INFO("BattleSystem", "Player unit " + std::to_string(event.unitId) + " defeated");
    } else if (enemyIt != activeEnemyUnits.end()) {
        activeEnemyUnits.erase(enemyIt);
        LOG_INFO("BattleSystem", "Enemy unit " + std::to_string(event.unitId) + " defeated");
    }

    CheckBattleEndConditions();
}

void BattleSystem::OnTurnEnd(const Events::TurnEndEvent& event) {
    if (currentState == BattleState::PlayerTurn) {
        currentState = BattleState::EnemyTurn;
    } else if (currentState == BattleState::EnemyTurn) {
        currentState = BattleState::PlayerTurn;
    }
}

void BattleSystem::CheckBattleEndConditions() {
    if (activePlayerUnits.empty()) {
        currentState = BattleState::Defeat;
        EndBattle(false);
    } else if (activeEnemyUnits.empty()) {
        currentState = BattleState::Victory;
        EndBattle(true);
    }
}

void BattleSystem::ProcessBattleLogic() {
    switch (currentState) {
        case BattleState::Initializing:
            break;
        case BattleState::PlayerTurn:
            break;
        case BattleState::EnemyTurn:
            break;
        case BattleState::Victory:
        case BattleState::Defeat:
            if (battleTimer > 3.0f) {
                EndBattle(currentState == BattleState::Victory);
            }
            break;
        case BattleState::Ended:
            break;
    }
}

} // namespace Elysium

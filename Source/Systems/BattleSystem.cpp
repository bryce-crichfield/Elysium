#include "Application.h"
#include "Systems/BattleSystem.h"
#include "Services/LogService.h"
#include "Services/EventService.h"

#include <algorithm>

namespace Elysium::Systems {

BattleSystem::BattleSystem(Context context)
    : System(context)
    , currentState(BattleState::Ended)
    , battleTimer(0.0f) {

    // Initialize event handlers
    auto& eventService = Application::GetInstance().GetService<Elysium::Services::EventService>("EventService");

    eventService.Subscribe<Events::AttackEvent>([this](const Events::AttackEvent& event) {
        OnAttackEvent(event);
    });

    eventService.Subscribe<Events::UnitKilledEvent>([this](const Events::UnitKilledEvent& event) {
        OnUnitKilled(event);
    });

    eventService.Subscribe<Events::TurnEndEvent>([this](const Events::TurnEndEvent& event) {
        OnTurnEnd(event);
    });
}

BattleSystem::~BattleSystem() {
}

void BattleSystem::Update(float deltaTime) {
    if (!IsBattleActive()) return;

    battleTimer += deltaTime;

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

void BattleSystem::StartBattle(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits) {
    currentState = BattleState::Initializing;
    battleTimer = 0.0f;
    currentBattleId = "battle_" + std::to_string(battleTimer);

    auto& eventService = Application::GetInstance().GetService<Elysium::Services::EventService>("EventService");
    Events::BattleStartEvent startEvent;
    startEvent.battleId = currentBattleId;
    eventService.FireEvent(startEvent);

    currentState = BattleState::PlayerTurn;
    LOG_INFO("BattleSystem", "Battle started with " + std::to_string(playerUnits.size()) + " player units vs " + std::to_string(enemyUnits.size()) + " enemy units");
}

void BattleSystem::EndBattle(bool playerVictory) {
    if (currentState == BattleState::Ended) return;

    currentState = BattleState::Ended;

    auto& eventService = Application::GetInstance().GetService<Elysium::Services::EventService>("EventService");
    Events::BattleEndEvent endEvent;
    endEvent.battleId = currentBattleId;
    endEvent.playerVictory = playerVictory;
    eventService.FireEvent(endEvent);

    LOG_INFO("BattleSystem", "Battle ended - " + std::string(playerVictory ? "Player Victory" : "Player Defeat"));
}

void BattleSystem::OnAttackEvent(const Events::AttackEvent& event) {
    LOG_DEBUG("BattleSystem", "Processing attack from unit " + std::to_string(event.entity) + " to unit " + std::to_string(event.target));
}

void BattleSystem::OnUnitKilled(const Events::UnitKilledEvent& event) {
    LOG_DEBUG("BattleSystem", "Unit " + std::to_string(event.unit) + " was killed");
}

void BattleSystem::OnTurnEnd(const Events::TurnEndEvent& event) {
    currentState = (currentState == BattleState::PlayerTurn) ? BattleState::EnemyTurn : BattleState::PlayerTurn;
}

} // namespace Elysium

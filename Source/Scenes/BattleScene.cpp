#include "Application.h"
#include "Services/LogService.h"
#include "Services/EventService.h"
#include "Scenes/BattleScene.h"
#include "Systems/BattleSystem.h"
#include "Systems/TurnSystem.h"
#include "Services/CharacterService.h"
#include "Systems/CursorSystem.h"
#include "System.h"

#include "imgui.h"
#include <memory>

namespace Elysium::Scenes {

BattleScene::BattleScene() : Scene() {
    // Systems will be initialized in OnEnter() with proper context
}

void BattleScene::OnEnter() {
    Scene::OnEnter();

    LOG_INFO("BattleScene", "Entering Battle Scene");

    // Register battle components before using them - stubbed out for now

    Context context = Context {
        .application = &Application::GetInstance(),
        .scene = this,
        .world = world_.get()
    };

    auto battleSys = std::make_unique<Systems::BattleSystem>(context);
    auto turnSys = std::make_unique<TurnSystem>(context);
    auto cursorSystem = std::make_unique<Elysium::Systems::CursorSystem>(context);

    battleSystem = battleSys.get();
    turnSystem = turnSys.get();

    this->AddSystem(std::move(battleSys));
    this->AddSystem(std::move(turnSys));
    this->AddSystem(std::move(cursorSystem));

    // Initialize event handlers
    auto& eventService = Application::GetInstance().GetService<Elysium::Services::EventService>("EventService");

    eventService.Subscribe<Events::BattleEndEvent>([this](const Events::BattleEndEvent& event) {
        OnBattleEnd(event);
    });

    // Add battle components to existing entities
    SetupBattleEntities();

    // Demo: Start a test battle with sample units
    std::vector<int> playerUnits = {1, 2};  // Sample character IDs
    std::vector<int> enemyUnits = {3, 4};   // Sample character IDs
    StartBattle(playerUnits, enemyUnits);
}

void BattleScene::OnUpdate(float deltaTime) {
    Scene::OnUpdate(deltaTime);

    if (battleSystem) {
        battleSystem->Update(deltaTime);
    }

    if (turnSystem) {
        turnSystem->Update(deltaTime);
    }
}

void BattleScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);
}

void BattleScene::OnExit() {
    LOG_INFO("BattleScene", "Exiting");
}

std::vector<Asset> BattleScene::GetAssets() {
    return {
        Asset(AssetType::MUSIC, "music", "sounds/music.mp3"),
        Asset(AssetType::TEXTURE, "mushroom_warrior_idle", "Sprites/mushroom_warrior/mushroom_warrior_idle.png"),
        Asset(AssetType::TEXTURE, "mushroom_warrior_walk", "Sprites/mushroom_warrior/mushroom_warrior_walk.png"),
        Asset(AssetType::SPRITE, "mushroom_warrior", "Sprites/mushroom_warrior/sprite.xml")
    };
}

void BattleScene::StartBattle(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits) {
    if (battleSystem) {
        battleSystem->StartBattle(playerUnits, enemyUnits);
    }
}

Systems::BattleState BattleScene::GetBattleState() const {
    return battleSystem ? battleSystem->GetCurrentState() : Systems::BattleState::Ended;
}

void BattleScene::OnBattleEnd(const Events::BattleEndEvent& event) {
    LOG_INFO("BattleScene", TextFormat("Battle ended: %s", event.playerVictory ? "Victory!" : "Defeat!"));
}

void BattleScene::SetupBattleEntities() {
    // Stubbed out - battle components were removed
}

}

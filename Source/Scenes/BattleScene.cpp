#include "Application.h"
#include "Scenes/BattleScene.h"
#include "Systems/BattleSystem.h"
#include "Systems/TurnSystem.h"
#include "Services/BattleEntityService.h"
#include "Services/CharacterService.h"
#include "Systems/CursorSystem.h"
#include "System.h"

#include "imgui.h"
#include <memory>

namespace Elysium::Scenes {

BattleScene::BattleScene() : Scene("BattleScene") {
    battleSystem = std::make_unique<BattleSystem>();
    turnSystem = std::make_unique<TurnSystem>();

}

void BattleScene::OnEnter() {
    Scene::OnEnter();

    TraceLog(LOG_INFO, "Entering Battle Scene");

    Context context = Context {
        .application = &Application::GetInstance(),
        .scene = this,
        .world = world_.get()
    };

    auto cursorSystem = std::make_unique<Elysium::Systems::CursorSystem>(context);
    this->AddSystem(std::move(cursorSystem));

    InitializeEventHandlers();

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

void BattleScene::OnDebugDraw() {
    Scene::OnDebugDraw();

    // Draw battle UI
    if (ImGui::Begin("Battle Debug")) {
        if (battleSystem) {
            BattleState state = battleSystem->GetCurrentState();
            ImGui::Text("Battle State: %d", static_cast<int>(state));
            ImGui::Text("Battle Active: %s", battleSystem->IsBattleActive() ? "Yes" : "No");
        }

        if (turnSystem) {
            ImGui::Text("Current Unit: %d", turnSystem->GetCurrentUnit());
            ImGui::Text("Turn Number: %d", turnSystem->GetTurnNumber());
            ImGui::Text("Player Turn: %s", turnSystem->IsPlayerTurn() ? "Yes" : "No");

            if (turnSystem->IsPlayerTurn() && turnSystem->HasActiveTurn()) {
                if (ImGui::Button("End Turn")) {
                    turnSystem->EndCurrentTurn();
                }
            }
        }
    }
    ImGui::End();
}

void BattleScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Battle Scene");
}

std::vector<Asset> BattleScene::GetAssets() {
    return {
        Asset(AssetType::MUSIC, "music", "./Assets/sounds/music.mp3"),
        Asset(AssetType::TEXTURE, "mushroom_warrior_idle", "./Assets/Sprites/mushroom_warrior/mushroom_warrior_idle.png"),
        Asset(AssetType::TEXTURE, "mushroom_warrior_walk", "./Assets/Sprites/mushroom_warrior/mushroom_warrior_walk.png"),
        Asset(AssetType::SPRITE, "mushroom_warrior", "./Assets/Sprites/mushroom_warrior/sprite.xml")
    };
}

void BattleScene::StartBattle(const std::vector<int>& playerUnits, const std::vector<int>& enemyUnits) {
    if (battleSystem) {
        battleSystem->StartBattle(playerUnits, enemyUnits);
    }
}

BattleState BattleScene::GetBattleState() const {
    return battleSystem ? battleSystem->GetCurrentState() : BattleState::Ended;
}

void BattleScene::InitializeEventHandlers() {
    auto& eventService = Application::GetInstance().GetEventService();

    eventService.Subscribe<Events::BattleEndEvent>([this](const Events::BattleEndEvent& event) {
        OnBattleEnd(event);
    });
}

void BattleScene::OnBattleEnd(const Events::BattleEndEvent& event) {
    TraceLog(LOG_INFO, TextFormat("Battle ended: %s", event.playerVictory ? "Victory!" : "Defeat!"));
}

}

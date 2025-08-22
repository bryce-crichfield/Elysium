#include "Application.h"
#include "Services/LogService.h"
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
    auto& eventService = Application::GetInstance().GetEventService();

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

void BattleScene::OnDebugDraw() {
    Scene::OnDebugDraw();

    // Draw battle UI
    if (ImGui::Begin("Battle Manager")) {
        if (ImGui::CollapsingHeader("Battle Status", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (battleSystem) {
                Systems::BattleState state = battleSystem->GetCurrentState();
                ImGui::Text("Battle State: %d", static_cast<int>(state));
                ImGui::Text("Battle Active: %s", battleSystem->IsBattleActive() ? "Yes" : "No");
            }

            if (turnSystem) {
                ImGui::Text("Current Unit: %d", turnSystem->GetCurrentUnit());
                ImGui::Text("Turn Number: %d", turnSystem->GetTurnNumber());
                ImGui::Text("Player Turn: %s", turnSystem->IsPlayerTurn() ? "Yes" : "No");
            }
        }

        if (ImGui::CollapsingHeader("Unit Management", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Unit management stubbed out - battle components removed");
        }

        if (ImGui::CollapsingHeader("Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (turnSystem && turnSystem->IsPlayerTurn() && turnSystem->HasActiveTurn()) {
                int currentUnit = turnSystem->GetCurrentUnit();
                ImGui::Text("Active Unit: %d", currentUnit);

                if (selectedUnit != -1) {
                    ImGui::Separator();
                    ImGui::Text("Selected Target: Unit %d", selectedUnit);

                    if (ImGui::Button("Attack Selected")) {
                        // TODO: Implement attack action
                        LOG_INFOF("BattleScene", "Unit %d attacks Unit %d", currentUnit, selectedUnit);
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Move To Target")) {
                        // TODO: Implement move action
                        LOG_INFOF("BattleScene", "Unit %d moves toward Unit %d", currentUnit, selectedUnit);
                    }
                }

                ImGui::Separator();
                if (ImGui::Button("End Turn")) {
                    turnSystem->EndCurrentTurn();
                    selectedUnit = -1; // Clear selection
                }
            } else {
                ImGui::Text("Waiting for player turn...");
            }
        }

        if (ImGui::CollapsingHeader("Battle Log")) {
            ImGui::BeginChild("BattleLogScrolling", ImVec2(0, 100), true);
            // TODO: Display action history
            ImGui::Text("Battle started...");
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void BattleScene::OnExit() {
    LOG_INFO("BattleScene", "Exiting");
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

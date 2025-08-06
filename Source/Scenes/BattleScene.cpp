#include "Scenes/MenuScene.h"
#include "Application.h"
#include "Scenes/GameScene.h"
#include "Scenes/BattleScene.h"
#include "imgui.h"
#include <memory>
#include "tinyxml2.h"

using namespace tinyxml2;

#define FOREACH(xmlName, varname, list) for (XMLElement* varname = list->FirstChildElement(xmlName); varname != nullptr; varname = varname->NextSiblingElement(xmlName))

namespace Elysium::Scenes {



BattleScene::BattleScene() : Scene("BattleScene") {
    // Base class constructor already creates world_ and systems_
}

void BattleScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Battle Scene");
}

void BattleScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Menu Scene");
}

void BattleScene::OnInput(const InputEvent& event) {
  
}

void BattleScene::OnUpdate(float deltaTime) {
    Scene::OnUpdate(deltaTime);
}

void BattleScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);
    
    auto& assetService = Elysium::Application::GetInstance().GetAssetService();
    
    // Load textures by name
    warriorTexture_ = assetService.GetTexture("warrior");
    swordTexture_ = assetService.GetTexture("sword");
    shieldTexture_ = assetService.GetTexture("shield");
    battlegroundTexture_ = assetService.GetTexture("battleground");

    // Draw battleground background
    if (battlegroundTexture_.id != 0) {
        DrawTexture(battlegroundTexture_, 0, 0, WHITE);
    }
    
    // Draw warrior at center-left
    if (warriorTexture_.id != 0) {
        DrawTexture(warriorTexture_, screen.width / 4, screen.height / 2 - 50, WHITE);
    }
    
    // Draw sword and shield near warrior
    if (swordTexture_.id != 0) {
        DrawTexture(swordTexture_, screen.width / 4 + 50, screen.height / 2 - 30, WHITE);
    }
    if (shieldTexture_.id != 0) {
        DrawTexture(shieldTexture_, screen.width / 4 - 30, screen.height / 2 - 20, WHITE);
    }

}

void BattleScene::OnDebugDraw() {
    // ImGui Scene Manager Window
    ImGui::Begin("Scene Manager");
    
    ImGui::Text("Current Scene: %s", GetName().c_str());
    ImGui::Separator();
    
    ImGui::Text("Available Scenes:");
    
    if (ImGui::Button("Switch to Game Scene", ImVec2(200, 30))) {
        auto gameScene = std::make_unique<GameScene>();
        Elysium::Application::GetInstance().QueueScene(std::move(gameScene));
    }

    if (ImGui::Button("Switch to Battle Scene", ImVec2(200, 30))) {
        // Reload current scene from XML
        Elysium::Application::GetInstance().QueueScene("./Assets/Scene.xml");
    }
    
    ImGui::Separator();
    
    ImGui::End();
}

std::vector<Asset> BattleScene::GetAssets() {
    return {
        Asset(AssetType::TEXTURE, "warrior", "./Assets/textures/warrior.png"),
        Asset(AssetType::TEXTURE, "sword", "./Assets/textures/sword.png"),
        Asset(AssetType::TEXTURE, "shield", "./Assets/textures/shield.png"),
        Asset(AssetType::TEXTURE, "battleground", "./Assets/textures/battleground.png"),
        Asset(AssetType::MUSIC, "music", "./Assets/sounds/music.mp3")
    };
}
} // namespace Elysium::Scenes
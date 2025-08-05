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



BattleScene::BattleScene(const GameConfig& config) : Scene("BattleScene", config) {
    // Base class constructor already creates world_ and systems_
}

void BattleScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Battle Scene");
    // XML loading is now handled by the Application when using QueueScene
    // or can be called manually with LoadFromXML if needed
    
    // Assets should already be loaded by the loading system at this point
    // Just try to retrieve them
    LoadAssets();
}

void BattleScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Menu Scene");
}

void BattleScene::OnInput(const InputEvent& event) {
  
}

void BattleScene::OnUpdate(float deltaTime) {
    // Call base class update which handles all systems
    Scene::OnUpdate(deltaTime);
}

void BattleScene::OnDraw() {
    // Call base class draw which renders all systems
    Scene::OnDraw();
    
    // Draw battleground background
    if (battlegroundTexture_.id != 0) {
        DrawTexture(battlegroundTexture_, 0, 0, WHITE);
    }
    
    // Draw warrior at center-left
    if (warriorTexture_.id != 0) {
        DrawTexture(warriorTexture_, config_.framebufferWidth / 4, config_.framebufferHeight / 2 - 50, WHITE);
    }
    
    // Draw sword and shield near warrior
    if (swordTexture_.id != 0) {
        DrawTexture(swordTexture_, config_.framebufferWidth / 4 + 50, config_.framebufferHeight / 2 - 30, WHITE);
    }
    if (shieldTexture_.id != 0) {
        DrawTexture(shieldTexture_, config_.framebufferWidth / 4 - 30, config_.framebufferHeight / 2 - 20, WHITE);
    }

}

void BattleScene::OnDebugDraw() {
    // ImGui Scene Manager Window
    ImGui::Begin("Scene Manager");
    
    ImGui::Text("Current Scene: %s", GetName().c_str());
    ImGui::Separator();
    
    ImGui::Text("Available Scenes:");
    
    if (ImGui::Button("Switch to Game Scene", ImVec2(200, 30))) {
        auto gameScene = std::make_unique<GameScene>(config_);
        Elysium::Application::GetInstance().QueueSceneTransition(std::move(gameScene));
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


void BattleScene::LoadAssets() {
    TraceLog(LOG_ERROR, "BattleScene::LoadAsset()");
    if (assetsLoaded_) {
        return; // Already loaded
    }
    
    auto& assetService = Elysium::Application::GetInstance().GetAssetService();
    
    // Load textures by name
    warriorTexture_ = assetService.GetTexture("warrior");
    swordTexture_ = assetService.GetTexture("sword");
    shieldTexture_ = assetService.GetTexture("shield");
    battlegroundTexture_ = assetService.GetTexture("battleground");
    
    // Load sounds by name
    // battleMusic_ = assetService.GetSound("music");

    // PlaySound(battleMusic_);

    TraceLog(LOG_INFO, "Playing battle music");
}

} // namespace Elysium::Scenes
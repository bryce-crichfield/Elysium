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


} // namespace Elysium::Scenes
#include "Scenes/MenuScene.h"
#include <memory>
#include "Core/Application.h"
#include "Services/LogService.h"

namespace Elysium::Scenes {

MenuScene::MenuScene() : Scene() {
    rotation_ = 0.0f;
    backgroundColor_ = DARKBLUE;
}

void MenuScene::OnEnter() {
    LOG_INFO("MenuScene", "Entering MenuScene");
}

void MenuScene::OnExit() {
    LOG_INFO("MenuScene", "Exiting MenuScene");
}

void MenuScene::OnUpdate(float deltaTime) {
    // Let base class run systems
    Scene::OnUpdate(deltaTime);
}

void MenuScene::OnDraw(Rectangle screen) {
    // Draw background
    ClearBackground(backgroundColor_);

    // Let base class render via RenderSystem
    Scene::OnDraw(screen);
}

void MenuScene::OnEvent(Event& event) {
    // Let base class forward events to systems (PickSystem, ScriptSystem, etc.)
    Scene::OnEvent(event);
}

}  // namespace Elysium::Scenes

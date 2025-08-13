#include "Scenes/OverworldScene.h"
#include "Scenes/MenuScene.h"
#include "Application.h"

namespace Elysium::Scenes {

OverworldScene::OverworldScene() : Scene("OverworldScene") {
}

void OverworldScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Overworld Scene");
}

void OverworldScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Overworld Scene");
}

void OverworldScene::OnUpdate(float deltaTime) {
    if (IsKeyPressed(KEY_BACKSPACE)) {
        auto menuScene = std::make_unique<MenuScene>();
        Elysium::Application::GetInstance().QueueScene(std::move(menuScene));
    }
}

void OverworldScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);

    DrawText("Backspace to Menu", 0, 0, 16, WHITE);
}

void OverworldScene::OnDebugDraw() {
}

} // namespace Elysium::Scenes

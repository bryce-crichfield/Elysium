#include "Scenes/OverworldScene.h"
#include "Services/LogService.h"
#include "Scenes/MenuScene.h"
#include "Application.h"

namespace Elysium::Scenes {

OverworldScene::OverworldScene() : Scene() {
}

void OverworldScene::OnEnter() {
    LOG_INFO("OverworldScene", "Entering scene");
}

void OverworldScene::OnExit() {
    LOG_INFO("OverworldScene", "Exiting scene");
}

void OverworldScene::OnUpdate(float deltaTime) {
    if (IsKeyPressed(KEY_BACKSPACE)) {
        Elysium::Application::GetInstance().QueueScene("MenuScene");
    }
}

void OverworldScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);

    DrawText("Backspace to Menu", 0, 0, 16, WHITE);
}

} // namespace Elysium::Scenes

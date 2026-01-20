#include "Scenes/OverworldScene.h"
#include "Application.h"
#include "Scenes/MenuScene.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "imgui.h"

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
    Scene::OnUpdate(deltaTime);

    // Don't process input if ImGui is capturing keyboard
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        auto& sceneService = Elysium::Application::GetInstance().GetService<Elysium::Services::SceneService>();
        sceneService.Replace("MenuScene");
    }
}

void OverworldScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);

    DrawText("Backspace to Menu", 0, 0, 16, WHITE);
}

}  // namespace Elysium::Scenes

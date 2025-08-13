#include "Scenes/ExploreScene.h"
#include "Scenes/MenuScene.h"
#include "Application.h"

namespace Elysium::Scenes {

ExploreScene::ExploreScene() : Scene("ExploreScene") {
}

void ExploreScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Overworld Scene");
}

void ExploreScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Overworld Scene");
}

void ExploreScene::OnUpdate(float deltaTime) {
    if (IsKeyPressed(KEY_BACKSPACE)) {
        auto menuScene = std::make_unique<MenuScene>();
        Elysium::Application::GetInstance().QueueScene(std::move(menuScene));
    }
}

void ExploreScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);

    DrawText("Backspace to Menu", 0, 0, 16, WHITE);
}

void ExploreScene::OnDebugDraw() {
}

} // namespace Elysium::Scenes

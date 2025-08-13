#include "Scenes/GameScene.h"

namespace Elysium::Scenes {

GameScene::GameScene() : Scene("GameScene") {
}

void GameScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Overworld Scene");
}

void GameScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Overworld Scene");
}

void GameScene::OnUpdate(float deltaTime) {
}

void GameScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);
}

void GameScene::OnDebugDraw() {
}

} // namespace Elysium::Scenes

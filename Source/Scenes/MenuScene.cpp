#include "Scenes/MenuScene.h"
#include "Services/LogService.h"
#include "Scenes/ExploreScene.h"
#include "Scenes/BattleScene.h"
#include "Scenes/OverworldScene.h"
#include "Application.h"
#include <memory>

namespace Elysium::Scenes {

MenuScene::MenuScene() : Scene() {
    rotation_ = 0.0f;
    backgroundColor_ = DARKBLUE;
}

void MenuScene::OnEnter() {
    LOG_INFO("MenuScene", "Entering scene");
}

void MenuScene::OnExit() {
    LOG_INFO("MenuScene", "Exiting scene");
}

void MenuScene::OnUpdate(float deltaTime) {
    rotation_ += deltaTime * 30.0f;
    if (rotation_ >= 360.0f) rotation_ -= 360.0f;
}

void MenuScene::OnDraw(Rectangle screen) {
    // Draw some rotating rectangles as background
    int centerX = screen.width / 2;
    int centerY = screen.height / 2;

    for (int i = 0; i < 5; i++) {
        float size = 50.0f + i * 20.0f;
        Color color = {
            (unsigned char)(100 + i * 30),
            (unsigned char)(50 + i * 20),
            (unsigned char)(150 + i * 10),
            100
        };

        Vector2 origin = { size / 2, size / 2 };
        Rectangle rect = { centerX - size/2, centerY - size/2, size, size };
        DrawRectanglePro(rect, origin, rotation_ + i * 45.0f, color);
    }

    DrawText("MENU SCENE", centerX - 120, centerY - 200, 40, RAYWHITE);
    DrawText("Use the Scene Manager window to switch scenes", centerX - 200, centerY + 150, 20, LIGHTGRAY);
}

} // namespace Elysium::Scenes

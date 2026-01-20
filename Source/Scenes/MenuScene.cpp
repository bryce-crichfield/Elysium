#include "Scenes/MenuScene.h"
#include <memory>
#include "Application.h"
#include "Scenes/ExploreScene.h"
#include "Scenes/OverworldScene.h"
#include "Services/LogService.h"

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
    if (rotation_ >= 360.0f)
        rotation_ -= 360.0f;

    // Update click effects
    for (auto it = clickEffects_.begin(); it != clickEffects_.end();) {
        it->lifetime -= deltaTime;
        if (it->lifetime <= 0.0f) {
            it = clickEffects_.erase(it);
        } else {
            ++it;
        }
    }
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
            100};

        Vector2 origin = {size / 2, size / 2};
        Rectangle rect = {centerX - size / 2, centerY - size / 2, size, size};
        DrawRectanglePro(rect, origin, rotation_ + i * 45.0f, color);
    }

    DrawText("MENU SCENE", centerX - 120, centerY - 200, 40, RAYWHITE);
    DrawText("Use the Scene Manager window to switch scenes", centerX - 200, centerY + 150, 20, LIGHTGRAY);
    DrawText("Click anywhere in the framebuffer to see events!", centerX - 230, centerY + 180, 20, YELLOW);

    // Draw click effects
    for (const auto& effect : clickEffects_) {
        float alpha = effect.lifetime / 1.0f;   // Fade out over 1 second
        float radius = (1.0f - alpha) * 30.0f;  // Expand outward
        Color color = effect.color;
        color.a = (unsigned char)(alpha * 255);
        DrawCircleV(effect.position, radius, color);
        DrawCircleLines(effect.position.x, effect.position.y, radius, color);
    }
}

void MenuScene::OnEvent(Event& event) {
    // Handle mouse button pressed
    if (event.Is<MouseButtonPressedEvent>()) {
        auto* mouseEvent = event.As<MouseButtonPressedEvent>();
        Vector2 pos = mouseEvent->GetPosition();

        // Choose color based on button
        Color color = RAYWHITE;
        if (mouseEvent->GetButton() == MOUSE_LEFT_BUTTON) {
            color = GREEN;
        } else if (mouseEvent->GetButton() == MOUSE_RIGHT_BUTTON) {
            color = RED;
        } else if (mouseEvent->GetButton() == MOUSE_MIDDLE_BUTTON) {
            color = BLUE;
        }

        clickEffects_.push_back({pos, 1.0f, color});

        LOG_INFOF("MenuScene", "Mouse clicked at (%.1f, %.1f), button %d",
                  pos.x, pos.y, mouseEvent->GetButton());

        event.handled = true;  // Mark as handled
    }

    // Handle key pressed
    if (event.Is<KeyPressedEvent>()) {
        auto* keyEvent = event.As<KeyPressedEvent>();
        LOG_INFOF("MenuScene", "Key pressed: %d", keyEvent->GetKey());
    }
}

}  // namespace Elysium::Scenes

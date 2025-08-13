#include "Scenes/MenuScene.h"
#include "Scenes/ExploreScene.h"
#include "Scenes/BattleScene.h"
#include "Scenes/OverworldScene.h"
#include "Application.h"
#include "imgui.h"
#include <memory>

namespace Elysium::Scenes {

MenuScene::MenuScene() : Scene("MenuScene") {
    rotation_ = 0.0f;
    backgroundColor_ = DARKBLUE;
}

void MenuScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Menu Scene");
}

void MenuScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Menu Scene");
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

void MenuScene::OnDebugDraw() {
    // ImGui Scene Manager Window
    ImGui::Begin("Scene Manager");

    ImGui::Text("Current Scene: %s", GetName().c_str());
    ImGui::Separator();

    ImGui::Text("Available Scenes:");

    if (ImGui::Button("Switch to Overworld Scene", ImVec2(200, 30))) {
        auto overworldScene = std::make_unique<OverworldScene>();
        Elysium::Application::GetInstance().QueueScene(std::move(overworldScene));
    }

    if (ImGui::Button("Switch to Explore Scene", ImVec2(200, 30))) {
        auto exploreScene = std::make_unique<ExploreScene>();
        Elysium::Application::GetInstance().QueueScene(std::move(exploreScene));
    }

    if (ImGui::Button("Switch to Battle Scene", ImVec2(200, 30))) {
        // Use the new XML loading system
        Elysium::Application::GetInstance().QueueScene("./Assets/Scene.xml");
    }

    ImGui::Separator();

    ImGui::Text("Menu Scene Controls:");
    ImGui::ColorEdit3("Background Color", (float*)&backgroundColor_);

    if (ImGui::Button("Reset Rotation")) {
        rotation_ = 0.0f;
    }

    ImGui::Text("Rotation: %.1f degrees", rotation_);

    ImGui::Separator();

    auto& jukebox = Application::GetInstance().GetJukeboxService();

    ImGui::Text("JukeBox: %s", jukebox.IsPlaying() ? "true" : "false");
    ImGui::Text("Current Song ID: %s", jukebox.GetCurrentSongId());


    ImGui::End();
}

} // namespace Elysium::Scenes

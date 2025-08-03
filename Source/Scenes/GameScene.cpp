#include "Scenes/GameScene.h"
#include "Application.h"
#include "Scenes/MenuScene.h"
#include "imgui.h"
#include <memory>
#include <random>

namespace Elysium::Scenes {

GameScene::GameScene() : Scene("GameScene") {
    gravity_ = 500.0f;
    paused_ = false;
    ballCount_ = 0;
    
    // Add a few initial balls
    for (int i = 0; i < 3; i++) {
        AddBall();
    }
}

void GameScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Game Scene");
}

void GameScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Game Scene");
}

void GameScene::OnUpdate(float deltaTime) {
    if (paused_) return;
    
    for (auto& ball : balls_) {
        // Apply gravity
        ball.velocity.y += gravity_ * deltaTime;
        
        // Update position
        ball.position.x += ball.velocity.x * deltaTime;
        ball.position.y += ball.velocity.y * deltaTime;
        
        // Bounce off walls
        if (ball.position.x <= ball.radius || ball.position.x >= GetScreenWidth() - ball.radius) {
            ball.velocity.x *= -0.8f; // Add some damping
            ball.position.x = (ball.position.x <= ball.radius) ? ball.radius : GetScreenWidth() - ball.radius;
        }
        
        if (ball.position.y <= ball.radius || ball.position.y >= GetScreenHeight() - ball.radius) {
            ball.velocity.y *= -0.8f; // Add some damping
            ball.position.y = (ball.position.y <= ball.radius) ? ball.radius : GetScreenHeight() - ball.radius;
        }
    }
}

void GameScene::OnDraw() {
    // Draw balls
    for (const auto& ball : balls_) {
        DrawCircleV(ball.position, ball.radius, ball.color);
        DrawCircleLinesV(ball.position, ball.radius, BLACK);
    }
    
    DrawText("GAME SCENE", 10, 10, 30, RAYWHITE);
    DrawText("Physics Simulation", 10, 50, 20, LIGHTGRAY);
    
    if (paused_) {
        int centerX = GetScreenWidth() / 2;
        int centerY = GetScreenHeight() / 2;
        DrawText("PAUSED", centerX - 60, centerY, 30, RED);
    } 
}

void GameScene::OnDebugDraw()
{
    // ImGui Controls
    ImGui::Begin("Scene Manager");
    
    ImGui::Text("Current Scene: %s", GetName().c_str());
    ImGui::Separator();
    
    ImGui::Text("Available Scenes:");
    
    if (ImGui::Button("Switch to Menu Scene", ImVec2(200, 30))) {
        auto menuScene = std::make_unique<MenuScene>();
        Elysium::Application::GetInstance().QueueSceneTransition(std::move(menuScene));
    }
    
    ImGui::Separator();
    
    ImGui::Text("Game Controls:");
    ImGui::Checkbox("Paused", &paused_);
    ImGui::SliderFloat("Gravity", &gravity_, 0.0f, 1000.0f);
    
    ImGui::Text("Balls: %d", (int)balls_.size());
    
    if (ImGui::Button("Add Ball")) {
        AddBall();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Add Ball x100")) {
        for (int i = 0; i < 100; i++) {
            AddBall();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear Balls")) {
        ClearBalls();
    }

    
    
    ImGui::Text("Controls:");
    ImGui::BulletText("M - Return to Menu");
    ImGui::BulletText("Space - Pause/Unpause");
    ImGui::BulletText("Click - Add ball at mouse");
    
    ImGui::End();
}

void GameScene::OnInput(const InputEvent& event) {
    if (event.type == InputEvent::KEY_PRESS) {
        if (event.key == KEY_M) {
            // Switch to menu scene
            auto menuScene = std::make_unique<MenuScene>();
            Elysium::Application::GetInstance().QueueSceneTransition(std::move(menuScene));
        } else if (event.key == KEY_SPACE) {
            paused_ = !paused_;
        }
    } else if (event.type == InputEvent::MOUSE_PRESS) {
        // Add ball at mouse position
        Ball ball;
        ball.position = { event.x, event.y };
        ball.velocity = { 
            (float)(GetRandomValue(-200, 200)), 
            (float)(GetRandomValue(-100, 50)) 
        };
        ball.radius = (float)GetRandomValue(10, 30);
        ball.color = {
            (unsigned char)GetRandomValue(100, 255),
            (unsigned char)GetRandomValue(100, 255),
            (unsigned char)GetRandomValue(100, 255),
            255
        };
        balls_.push_back(ball);
    }
}

void GameScene::AddBall() {
    Ball ball;
    ball.position = { 
        (float)GetRandomValue(50, GetScreenWidth() - 50), 
        (float)GetRandomValue(50, 200) 
    };
    ball.velocity = { 
        (float)GetRandomValue(-200, 200), 
        (float)GetRandomValue(-100, 50) 
    };
    ball.radius = (float)GetRandomValue(10, 30);
    ball.color = {
        (unsigned char)GetRandomValue(100, 255),
        (unsigned char)GetRandomValue(100, 255),
        (unsigned char)GetRandomValue(100, 255),
        255
    };
    balls_.push_back(ball);
}

void GameScene::ClearBalls() {
    balls_.clear();
}

} // namespace Elysium::Scenes
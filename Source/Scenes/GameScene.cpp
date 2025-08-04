#include "Scenes/GameScene.h"
#include "Application.h"
#include "Scenes/MenuScene.h"
#include "imgui.h"
#include <memory>
#include <random>

namespace Elysium::Scenes {


GameScene::GameScene(const GameConfig& config) : Scene("GameScene", config) {
    world_ = std::make_unique<EntityWorld>();
    physicsSystem_ = std::make_unique<Elysium::Systems::PhysicsSystem>(world_.get());
    renderSystem_ = std::make_unique<Elysium::Systems::RenderSystem>(world_.get());
    paused_ = false;
    
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
    if (!paused_) {
        physicsSystem_->Update(deltaTime);
    }
}

void GameScene::OnDraw() {
    renderSystem_->Render();
    
    DrawText("GAME SCENE", 10, 10, 30, RAYWHITE);
    DrawText("Physics Simulation", 10, 50, 20, LIGHTGRAY);
    
    if (paused_) {
        int centerX = config_.GetFramebufferCenterX();
        int centerY = config_.GetFramebufferCenterY();
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
        auto menuScene = std::make_unique<MenuScene>(Elysium::Application::GetInstance().GetConfig());
        Elysium::Application::GetInstance().QueueSceneTransition(std::move(menuScene));
    }
    
    ImGui::Separator();
    
    ImGui::Text("Game Controls:");
    ImGui::Checkbox("Paused", &paused_);
    float gravity = physicsSystem_->GetGravity();
    if (ImGui::SliderFloat("Gravity", &gravity, 0.0f, 1000.0f)) {
        physicsSystem_->SetGravity(gravity);
    }
    
    ImGui::Text("Balls: %d", (int)world_->GetEntityCount());
    
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
            auto menuScene = std::make_unique<MenuScene>(Elysium::Application::GetInstance().GetConfig());
            Elysium::Application::GetInstance().QueueSceneTransition(std::move(menuScene));
        } else if (event.key == KEY_SPACE) {
            paused_ = !paused_;
        }
    } else if (event.type == InputEvent::MOUSE_PRESS) {
        AddBallAtPosition(event.x, event.y);
    }
}

void GameScene::AddBall() {
    float x = (float)GetRandomValue(50, config_.GetFramebufferWidth() - 50);
    float y = (float)GetRandomValue(50, 200);
    AddBallAtPosition(x, y);
}

void GameScene::AddBallAtPosition(float x, float y) {
    Entity ball = world_->CreateEntity();
    
    world_->AddComponent(ball, PositionComponent(x, y));
    world_->AddComponent(ball, VelocityComponent(
        (float)GetRandomValue(-200, 200),
        (float)GetRandomValue(-100, 50)
    ));
    
    float radius = (float)GetRandomValue(10, 30);
    Color color = {
        (unsigned char)GetRandomValue(100, 255),
        (unsigned char)GetRandomValue(100, 255),
        (unsigned char)GetRandomValue(100, 255),
        255
    };
    
    world_->AddComponent(ball, CircleRenderComponent(radius, color));
    world_->AddComponent(ball, PhysicsComponent());
}

void GameScene::ClearBalls() {
    // Use the vector-based approach here since we need to collect entities before destroying them
    // (can't destroy entities while iterating over them)
    auto& entities = world_->GetEntitiesWithComponents<PositionComponent, VelocityComponent, CircleRenderComponent>();
    for (Entity entity : entities) {
        world_->DestroyEntity(entity);
    }
}

} // namespace Elysium::Scenes
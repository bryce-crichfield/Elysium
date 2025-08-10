#include "Scenes/GameScene.h"
#include "Application.h"
#include "Scenes/MenuScene.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/AnimationSystem.h"
#include "imgui.h"
#include <memory>
#include <cmath>
#include "Animation.h"

namespace Elysium::Scenes {

GameScene::GameScene() : Scene("GameScene") {
    player_ = INVALID_ENTITY;
    house_ = INVALID_ENTITY;
    lastTargetPos_ = {0, 0};

    // Create the systems for this scene
    CreateCustomSystems();

    // Create the overworld entities
    CreatePlayer();
    CreateHouse();
}

void GameScene::CreateCustomSystems() {
    // Add render system if not already loaded from XML
    bool hasRender = false;
    bool hasAnimation = false;

    // for (const auto& system : systems_) {
    //     if (dynamic_cast<Elysium::Systems::RenderSystem*>(system.get())) {
    //         hasRender = true;
    //     }
    //     if (dynamic_cast<Elysium::Systems::AnimationSystem*>(system.get())) {
    //         hasAnimation = true;
    //     }
    // }

    // if (!hasRender) {
    //     systems_.push_back(std::make_unique<Elysium::Systems::RenderSystem>(world_.get()));
    // }
    // if (!hasAnimation) {
    //     systems_.push_back(std::make_unique<Elysium::AnimationSystem>(world_.get()));
    // }
}

void GameScene::CreatePlayer() {
    player_ = world_->CreateEntity("Player");

    // Start at grid position (3, 3)
    Vector2 startPos = {3 * GRID_SIZE + GRID_SIZE/2, 3 * GRID_SIZE + GRID_SIZE/2};
    world_->AddComponent(player_, PositionComponent(startPos.x, startPos.y));
    world_->AddComponent(player_, CircleComponent(12.0f, RED, DARKGRAY));

    // Add animation component for smooth movement
    world_->AddComponent(player_, AnimationComponent());

    // Initialize last target position
    lastTargetPos_ = startPos;
}

void GameScene::CreateHouse() {
    house_ = world_->CreateEntity("House");

    // Place house at grid position (8, 5) - larger box to represent a house
    Vector2 housePos = {8 * GRID_SIZE, 5 * GRID_SIZE};
    world_->AddComponent(house_, PositionComponent(housePos.x, housePos.y));

    // Using CircleRenderComponent as a temporary box (we could create a RectangleRenderComponent later)
    // For now, use a large gray circle to represent the house
    world_->AddComponent(house_, CircleComponent(GRID_SIZE * 1.5f, BROWN, DARKBROWN));
}

void GameScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Overworld Scene");
}

void GameScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Overworld Scene");
}

void GameScene::OnUpdate(float deltaTime) {
    // Handle keyboard input for player movement (polling since input events aren't hooked up yet)
    HandlePlayerMovement(deltaTime);

    // Call the base class update which runs all systems
    Scene::OnUpdate(deltaTime);
}

void GameScene::HandlePlayerMovement(float deltaTime) {
    if (!world_->HasComponent<PositionComponent>(player_) || !world_->HasComponent<AnimationComponent>(player_)) {
        return;
    }

    auto& animation = world_->GetComponent<AnimationComponent>(player_);

    // Limit queue size to prevent excessive queuing (max 3 moves ahead)
    const size_t MAX_QUEUE_SIZE = 3;
    size_t currentQueueSize = animation.actionQueue.size();
    if (animation.currentAction) currentQueueSize++;

    if (currentQueueSize >= MAX_QUEUE_SIZE) {
        return;
    }

    // Calculate current grid position - if animating, use target position from last queued action
    Vector2 currentGridPos;
    if (animation.currentAction || !animation.actionQueue.empty()) {
        // Get the target position of the last action in the queue
        Vector2 targetPos = GetLastTargetPosition();
        currentGridPos = {
            floorf(targetPos.x / GRID_SIZE),
            floorf(targetPos.y / GRID_SIZE)
        };
    } else {
        // Use actual current position
        auto& pos = world_->GetComponent<PositionComponent>(player_);
        currentGridPos = {
            floorf(pos.x / GRID_SIZE),
            floorf(pos.y / GRID_SIZE)
        };
    }

    Vector2 newGridPos = currentGridPos;

    // Poll keyboard input - use IsKeyDown for held keys instead of IsKeyPressed for single presses
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
        newGridPos.y -= 1;
    } else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
        newGridPos.y += 1;
    } else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        newGridPos.x -= 1;
    } else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        newGridPos.x += 1;
    }

    // If movement was requested, add animation to queue
    if (newGridPos.x != currentGridPos.x || newGridPos.y != currentGridPos.y) {
        // Clamp to reasonable bounds
        newGridPos.x = fmaxf(0.0f, fminf(19.0f, newGridPos.x));
        newGridPos.y = fmaxf(0.0f, fminf(14.0f, newGridPos.y));

        MovePlayerWithAnimation(
            newGridPos.x * GRID_SIZE + GRID_SIZE/2,
            newGridPos.y * GRID_SIZE + GRID_SIZE/2
        );
    }
}

void GameScene::MovePlayerWithAnimation(float newX, float newY) {
    if (!world_->HasComponent<AnimationComponent>(player_)) {
        return;
    }

    auto& animation = world_->GetComponent<AnimationComponent>(player_);

    // Create a smooth movement animation using Fx::MoveTo
    auto moveAction = Fx::MoveTo(newX, newY, 0.25f); // 0.25 second movement duration
    animation.actionQueue.push(moveAction);

    // Update our tracked target position
    lastTargetPos_ = {newX, newY};
}

Vector2 GameScene::GetLastTargetPosition() {
    auto& animation = world_->GetComponent<AnimationComponent>(player_);

    // If we have queued actions, return our tracked target position
    if (animation.currentAction || !animation.actionQueue.empty()) {
        return lastTargetPos_;
    }

    // Otherwise return current position
    auto& pos = world_->GetComponent<PositionComponent>(player_);
    return {pos.x, pos.y};
}

void GameScene::OnDraw(Rectangle screen) {
    // Draw grid background
    DrawGrid(screen);

    // Call the base class draw which renders all systems
    Scene::OnDraw(screen);

    // Draw UI
    DrawText("OVERWORLD", 10, 10, 30, RAYWHITE);
    DrawText("Hold WASD or Arrow Keys to move the red ball", 10, 50, 20, LIGHTGRAY);
    DrawText("M - Return to Menu", 10, 80, 16, LIGHTGRAY);
}

void GameScene::DrawGrid(Rectangle screen) {
    // Draw grid lines
    for (float x = 0; x < screen.width; x += GRID_SIZE) {
        DrawLine((int)x, 0, (int)x, (int)screen.height, ColorAlpha(DARKGRAY, 0.3f));
    }
    for (float y = 0; y < screen.height; y += GRID_SIZE) {
        DrawLine(0, (int)y, (int)screen.width, (int)y, ColorAlpha(DARKGRAY, 0.3f));
    }
}

void GameScene::OnDebugDraw() {
    ImGui::Begin("Overworld Scene");

    ImGui::Text("Current Scene: %s", GetName().c_str());
    ImGui::Separator();

    if (ImGui::Button("Switch to Menu Scene", ImVec2(200, 30))) {
        auto menuScene = std::make_unique<MenuScene>();
        Elysium::Application::GetInstance().QueueScene(std::move(menuScene));
    }

    ImGui::Separator();

    ImGui::Text("Player Info:");
    if (world_->HasComponent<PositionComponent>(player_)) {
        auto& pos = world_->GetComponent<PositionComponent>(player_);
        Vector2 gridPos = {
            floorf(pos.x / GRID_SIZE),
            floorf(pos.y / GRID_SIZE)
        };
        ImGui::Text("Position: (%.1f, %.1f)", pos.x, pos.y);
        ImGui::Text("Grid Position: (%.0f, %.0f)", gridPos.x, gridPos.y);

        if (world_->HasComponent<AnimationComponent>(player_)) {
            auto& animation = world_->GetComponent<AnimationComponent>(player_);
            bool isAnimating = animation.currentAction != nullptr || !animation.actionQueue.empty();
            size_t queueSize = animation.actionQueue.size();
            ImGui::Text("Animating: %s", isAnimating ? "Yes" : "No");
            ImGui::Text("Queue Size: %zu", queueSize);
            ImGui::Text("Last Target: (%.1f, %.1f)", lastTargetPos_.x, lastTargetPos_.y);
        }
    }

    ImGui::Separator();
    ImGui::Text("Controls:");
    ImGui::BulletText("Hold WASD or Arrow Keys - Move Player");
    ImGui::BulletText("M - Return to Menu");

    ImGui::End();
}

} // namespace Elysium::Scenes

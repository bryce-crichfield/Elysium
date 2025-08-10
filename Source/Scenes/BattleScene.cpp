#include "Scenes/MenuScene.h"
#include "Application.h"
#include "Scenes/GameScene.h"
#include "Scenes/BattleScene.h"
#include "Systems/AnimationSystem.h"
#include "imgui.h"
#include <memory>
#include <cmath>
#include "Animation.h"

namespace Elysium::Scenes {

BattleScene::BattleScene() : Scene("BattleScene") {
}

void BattleScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Battle Scene - Action Composition Demo");

    // // Setup Hero (RedBall) with composable action sequences
    // Entity hero;
    // if (world_->GetEntityByName("RedBall", &hero)) {
    //     auto animation = AnimationComponent();

    //     try {
    //         Elysium::ActionParser parser;
    //         parser.SetParameter("startX", "100");
    //         parser.SetParameter("startY", "200");
    //         parser.SetParameter("pauseTime", "1.5");
    //         parser.SetParameter("bounceCount", "4");
    //         parser.RegisterCallback("completionCallback", []() {
    //             TraceLog(LOG_INFO, "Action Complete");
    //         });

    //         auto action = parser.LoadFromXML("./Assets/Animation/Animation.xml");
    //         animation.actionQueue.push(action);
    //         TraceLog(LOG_INFO, "Successfully loaded action from XML");
    //     } catch (const std::exception& e) {
    //         TraceLog(LOG_ERROR, "Failed to load XML action: %s", e.what());
    //         // Fallback to a simple action
    //         animation.actionQueue.push(
    //             Fx::Sequence(
    //                 Fx::MoveTo(100, 200, 3.0f),
    //                 Fx::Wait(1.5f),
    //                 Fx::MoveBy(0, -20, 0.2f)
    //             )
    //         );
    //     }

    //     world_->AddComponent<AnimationComponent>(hero, animation);
    // }

    // Setup ENTITY_0 with patrol animation
    Entity patrolEntity;
    if (world_->GetEntityByName("ENTITY_0", &patrolEntity)) {
        // Get the existing AnimationComponent (loaded from XML) instead of creating new one
        auto& patrolAnimation = world_->GetComponent<AnimationComponent>(patrolEntity);

        // Create a patrol loop that moves ENTITY_0 around the map in a square pattern
        auto patrolLoop = Fx::Loop(
            []() { return true; }, // Always true = infinite loop
            Fx::Sequence(
                Fx::MoveTo(0, 0, 2.0f),  // Move to tile (0,0) first
                Fx::Wait(0.5f),          // Wait half second
                Fx::MoveTo(5, 0, 2.0f),  // Move to tile (5,0)
                Fx::Wait(0.5f),
                Fx::MoveTo(5, 5, 2.0f),  // Move to tile (5,5)
                Fx::Wait(0.5f),
                Fx::MoveTo(0, 5, 2.0f),  // Move to tile (0,5)
                Fx::Wait(0.5f)           // Complete the square
            )
        );

        patrolAnimation.actionQueue.push(patrolLoop);
    }
}

void BattleScene::OnUpdate(float deltaTime) {
    Scene::OnUpdate(deltaTime);
}

void BattleScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);
}

void BattleScene::OnDebugDraw() {
    Scene::OnDebugDraw();
}

void BattleScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Menu Scene");
}

std::vector<Asset> BattleScene::GetAssets() {
    return {
        Asset(AssetType::MUSIC, "music", "./Assets/sounds/music.mp3")
    };
}
}

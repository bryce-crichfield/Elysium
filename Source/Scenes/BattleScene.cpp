#include "Scenes/MenuScene.h"
#include "Application.h"
#include "Scenes/ExploreScene.h"
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
    // Call base class to handle deferred XML loading
    Scene::OnEnter();

    TraceLog(LOG_INFO, "Entering Battle Scene - Action Composition Demo");

    // Setup ENTITY_0 with patrol animation
    Entity patrolEntity;
    if (world_->GetEntityByName("ENTITY_0", &patrolEntity)) {
        // Get the existing AnimationComponent (loaded from XML) instead of creating new one
        auto& patrolAnimation = world_->GetComponent<AnimationComponent>(patrolEntity);

        // Create two parallel actions:
        // 1. Patrol movement with marker switching
        // 2. Continuous frame animation

        auto patrolSequence = Fx::Loop(
            []() { return true; }, // Always true = infinite loop
            Fx::Sequence(
                // Wait
                Fx::SwitchMarker("idle", "down"),
                Fx::Wait(1.0f),

                // Move right
                Fx::SwitchMarker("walk", "right"),
                Fx::MoveTo(5, 0, 2.0f),
                // Wait
                Fx::SwitchMarker("idle", "down"),
                Fx::Wait(1),

                // Move up
                Fx::SwitchMarker("walk", "up"),
                Fx::MoveTo(5, 5, 2.0f),
                // Wait
                Fx::SwitchMarker("idle", "down"),
                Fx::Wait(1),

                // Move left
                Fx::SwitchMarker("walk", "left"),
                Fx::MoveTo(0, 5, 2.0f),
                // Wait
                Fx::SwitchMarker("idle", "down"),
                Fx::Wait(1)
            )
        );

        patrolAnimation.actionQueue.push(patrolSequence);
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
        Asset(AssetType::MUSIC, "music", "./Assets/sounds/music.mp3"),

        Asset(AssetType::TEXTURE, "mushroom_warrior_idle", "./Assets/Sprites/mushroom_warrior/mushroom_warrior_idle.png"),
        Asset(AssetType::TEXTURE, "mushroom_warrior_walk", "./Assets/Sprites/mushroom_warrior/mushroom_warrior_walk.png"),
        Asset(AssetType::SPRITE, "mushroom_warrior", "./Assets/Sprites/mushroom_warrior/sprite.xml")
    };
}
}

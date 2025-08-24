#include "Scenes/ExploreScene.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "Scenes/MenuScene.h"
#include "Application.h"

namespace Elysium::Scenes {

ExploreScene::ExploreScene() : Scene() {
}

void ExploreScene::OnEnter() {
    Scene::OnEnter();
    LOG_INFO("ExploreScene", "Entering scene");

    Entity patrolEntity;
    if (world_->GetEntityByName("ENTITY_0", &patrolEntity)) {
        auto& anim = world_->GetComponent<AnimationComponent>(patrolEntity);
        auto& sprite = world_->GetComponent<SpriteComponent>(patrolEntity);

        std::string markerName = "idle/down";
        anim.marker = markerName;

        // Get frame range from sprite data
        auto frameRange = sprite.sprite.GetMarkerFrameRange(markerName);
        anim.start = frameRange.first;
        anim.end = frameRange.second;
        anim.currentFrame = anim.start;
        anim.frameDuration = 0.5f; // 300ms per frame
        anim.loop = true;
        anim.elapsed = 0;

        // Set initial sprite state
        sprite.markerName = markerName;
        sprite.frameIndex = anim.currentFrame;
    }
}

void ExploreScene::OnExit() {
    Scene::OnExit();
    LOG_INFO("ExploreScene", "Exiting scene");
}

void ExploreScene::OnUpdate(float deltaTime) {
    Scene::OnUpdate(deltaTime);

    if (IsKeyPressed(KEY_BACKSPACE)) {
        auto& sceneService = Elysium::Application::GetInstance().GetService<Elysium::Services::SceneService>("SceneService");
        sceneService.SetScene("MenuScene");
    }
}

void ExploreScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);

    DrawText("EXPLORE_SCENE - Backspace to Menu", 0, 0, 16, WHITE);
}

std::vector<Asset> ExploreScene::GetAssets() {
    return {
        Asset(AssetType::TEXTURE, "mushroom_warrior_idle", "./Assets/Sprites/mushroom_warrior/mushroom_warrior_idle.png"),
        Asset(AssetType::TEXTURE, "mushroom_warrior_walk", "./Assets/Sprites/mushroom_warrior/mushroom_warrior_walk.png"),
        Asset(AssetType::SPRITE, "mushroom_warrior", "./Assets/Sprites/mushroom_warrior/sprite.xml")
    };
}

} // namespace Elysium::Scenes

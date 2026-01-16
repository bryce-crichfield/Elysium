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

    // Don't process input if ImGui is capturing keyboard
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        auto& sceneService = Elysium::Application::GetInstance().GetService<Elysium::Services::SceneService>();
        sceneService.Replace("MenuScene");
    }
}

void ExploreScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);

    DrawText("EXPLORE_SCENE - Backspace to Menu", 0, 0, 16, WHITE);
}

} // namespace Elysium::Scenes

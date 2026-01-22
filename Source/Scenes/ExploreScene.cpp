#include "Scenes/ExploreScene.h"
#include "Core/Application.h"
#include "Scenes/MenuScene.h"
#include "Services/LogService.h"
#include "Services/NetworkService.h"
#include "Services/SceneService.h"
#include "Systems/ClientNetworkSystem.h"
#include "Systems/ServerNetworkSystem.h"

namespace Elysium::Scenes {

ExploreScene::ExploreScene() : Scene() {
}

void ExploreScene::SetupNetworkSystems() {
    auto& networkService = Application::GetInstance().GetService<Services::NetworkService>();
    auto mode = networkService.GetMode();

    Context context;
    context.application = &Application::GetInstance();
    context.scene = this;
    context.world = world_.get();

    if (mode == Services::NetworkMode::Server) {
        auto serverSystem = std::make_unique<ServerNetworkSystem>(context);
        serverNetworkSystem_ = serverSystem.get();
        AddSystem(std::move(serverSystem));
        LOG_INFO("ExploreScene", "Added ServerNetworkSystem");
    } else if (mode == Services::NetworkMode::Client) {
        auto clientSystem = std::make_unique<ClientNetworkSystem>(context);
        clientNetworkSystem_ = clientSystem.get();
        AddSystem(std::move(clientSystem));
        LOG_INFO("ExploreScene", "Added ClientNetworkSystem");
    }
}

void ExploreScene::OnEnter() {
    Scene::OnEnter();
    LOG_INFO("ExploreScene", "Entering scene");

    // Setup network systems based on current network mode
    SetupNetworkSystems();

    Entity patrolEntity;
    if (world_->GetEntityByName("ENTITY_0", &patrolEntity)) {
        if (world_->HasComponent<AnimationComponent>(patrolEntity) && world_->HasComponent<SpriteComponent>(patrolEntity)) {
            auto& anim = world_->GetComponent<AnimationComponent>(patrolEntity);
            auto& sprite = world_->GetComponent<SpriteComponent>(patrolEntity);

            std::string markerName = "idle/down";
            anim.marker = markerName;

            // Get frame range from sprite data
            auto frameRange = sprite.sprite.GetMarkerFrameRange(markerName);
            anim.start = frameRange.first;
            anim.end = frameRange.second;
            anim.currentFrame = anim.start;
            anim.frameDuration = 0.5f;  // 300ms per frame
            anim.loop = true;
            anim.elapsed = 0;

            // Set initial sprite state
            sprite.markerName = markerName;
            sprite.frameIndex = anim.currentFrame;
        }
    }
}

void ExploreScene::OnMessage(Message& message) {
    // Forward to base class which dispatches to all systems
    Scene::OnMessage(message);
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

}  // namespace Elysium::Scenes

#include "Scene.h"
#include <sstream>
#include <string>
#include "Application.h"
#include "Entity.h"
#include "Event.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "System.h"
#include "Systems/AnimationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/SpriteSystem.h"
#include "Utilities/Xml.h"
#include "raylib.h"
#include "tinyxml2.h"

namespace Elysium {

Scene::Scene() {
    world_ = std::make_unique<World>();

    Context context;
    context.application = &Application::GetInstance();
    context.scene = this;
    context.world = world_.get();
}

Scene::~Scene() {
}

void Scene::OnUpdate(float deltaTime) {
    for (auto& system : systems_) {
        if (system->IsEnabled())
            system->Update(deltaTime);
    }
}

void Scene::OnDraw(Rectangle screen) {
    // RenderSystem now handles all camera rendering internally
    // Just render all systems - no need for manual camera management
    for (auto& system : systems_) {
        if (system->IsVisible())
            system->Draw();
    }
}

void Scene::OnEvent(Event& event) {
    // Forward events to all systems
    for (auto& system : systems_) {
        if (event.handled)
            break;  // Stop propagation if handled
        system->OnEvent(event);
    }
}

void Scene::OnMessage(Message& message) {
    // Forward messages to all systems
    for (auto& system : systems_) {
        system->OnMessage(message);
    }
}

void Scene::AddSystem(std::unique_ptr<System> system) {
    systems_.emplace_back(std::move(system));
    LOG_DEBUGF("Scene", "Added system: %s", typeid(*systems_.back()).name());
}

}  // namespace Elysium

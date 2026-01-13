#include "Scene.h"
#include "Services/LogService.h"
#include "Services/AssetService.h"
#include "Entity.h"
#include "System.h"
#include "Systems/RenderSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/AnimationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/SpriteSystem.h"
#include "Systems/TimelineSystem.h"
#include "Timeline.h"
#include "tinyxml2.h"
#include "raylib.h"
#include <sstream>
#include <string>
#include "Xml.h"
#include "Application.h"


namespace Elysium {

Scene::Scene() {
    world_ = std::make_unique<World>();

    // Add TimelineSystem to all scenes by default
    Context context;
    context.application = &Application::GetInstance();
    context.scene = this;
    context.world = world_.get();
    AddSystem(std::make_unique<Systems::TimelineSystem>(context));
}

Scene::~Scene() {

}

void Scene::OnUpdate(float deltaTime) {
    for (auto& system : systems_) {
        system->Update(deltaTime);
    }
}

void Scene::OnDraw(Rectangle screen) {
    // RenderSystem now handles all camera rendering internally
    // Just render all systems - no need for manual camera management
    for (auto& system : systems_) {
        system->Draw();
    }
}

void Scene::AddSystem(std::unique_ptr<System> system) {
    systems_.emplace_back(std::move(system));
    LOG_DEBUGF("Scene", "Added system: %s", typeid(*systems_.back()).name());
}

Timeline* Scene::CreateTimeline(const std::string& name) {
    auto timeline = std::make_unique<Timeline>(name);
    Timeline* ptr = timeline.get();
    timelines_.emplace_back(std::move(timeline));
    LOG_DEBUGF("Scene", "Created timeline: %s", name.c_str());
    return ptr;
}

Timeline* Scene::GetTimeline(const std::string& name) {
    for (auto& timeline : timelines_) {
        if (timeline->GetName() == name) {
            return timeline.get();
        }
    }
    return nullptr;
}

void Scene::RemoveTimeline(const std::string& name) {
    timelines_.erase(
        std::remove_if(timelines_.begin(), timelines_.end(),
            [&name](const std::unique_ptr<Timeline>& timeline) {
                return timeline->GetName() == name;
            }),
        timelines_.end()
    );
    LOG_DEBUGF("Scene", "Removed timeline: %s", name.c_str());
}

} // namespace Elysium

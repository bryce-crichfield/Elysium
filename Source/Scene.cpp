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
#include "tinyxml2.h"
#include "raylib.h"
#include <sstream>
#include <string>
#include "Xml.h"
#include "Application.h"


namespace Elysium {

Scene::Scene() {
    world_ = std::make_unique<World>();
}

Scene::~Scene() {

}

void Scene::LoadFromXML(const std::string& xmlPath) {
    SceneSerializer serializer;
    serializer.LoadScene(*this, xmlPath);
}

void Scene::SaveToXML(const std::string& xmlPath) {
    SceneSerializer serializer;
    serializer.SaveScene(*this, xmlPath);
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

} // namespace Elysium

#include "Scene.h"
#include <algorithm>
#include <sstream>
#include <string>
#include "Application.h"
#include "Entity.h"
#include "Event.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "Services/ScriptService.h"
#include "System.h"
#include "Systems/CameraSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/SpriteSystem.h"
#include "Core/Xml.h"
#include "Core/Path.h"
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

    if (!sceneScriptPath_.empty()) {
        auto& scriptService = Application::GetInstance().GetService<Services::ScriptService>();
        scriptService.SetActiveWorld(world_.get());
        if (!isSceneScriptInitialized_) {
            if (scriptService.InitializeScene(Path(sceneScriptPath_))) {
                isSceneScriptInitialized_ = true;
            }
        } else {
            scriptService.UpdateScene(Path(sceneScriptPath_), deltaTime);
        }
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
    // Scene script gets first crack at events
    if (!sceneScriptPath_.empty() && isSceneScriptInitialized_) {
        auto& scriptService = Application::GetInstance().GetService<Services::ScriptService>();
        scriptService.SetActiveWorld(world_.get());
        scriptService.OnSceneEvent(Path(sceneScriptPath_), event);
    }

    // Forward events to all systems
    for (auto& system : systems_) {
        if (event.handled)
            break;
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

void Scene::RemoveSystem(System* system) {
    auto it = std::find_if(systems_.begin(), systems_.end(),
        [system](const std::unique_ptr<System>& s) { return s.get() == system; });
    if (it != systems_.end()) {
        LOG_DEBUGF("Scene", "Removing system: %s", typeid(**it).name());
        systems_.erase(it);
    }
}

std::vector<SceneLayer>& Scene::GetLayers() {
    return layers_;
}

const std::vector<SceneLayer>& Scene::GetLayers() const {
    return layers_;
}

SceneLayer* Scene::GetLayer(const std::string& name) {
    for (auto& layer : layers_) {
        if (layer.name == name) return &layer;
    }
    return nullptr;
}

const SceneLayer* Scene::GetLayer(const std::string& name) const {
    for (const auto& layer : layers_) {
        if (layer.name == name) return &layer;
    }
    return nullptr;
}

void Scene::AddLayer(const SceneLayer& layer) {
    // Replace if exists
    for (auto& existing : layers_) {
        if (existing.name == layer.name) {
            existing = layer;
            return;
        }
    }
    layers_.push_back(layer);
    // Keep sorted by zIndex
    std::sort(layers_.begin(), layers_.end(), [](const SceneLayer& a, const SceneLayer& b) {
        return a.zIndex < b.zIndex;
    });
}

}  // namespace Elysium

#include "Scene.h"
#include <algorithm>
#include <sstream>
#include <string>
#include "Core/Common.h"
#include "Core/Log.h"
#include "Core/ServiceLocator.h"
#include "Entity.h"
#include "Event.h"
#include "Interfaces/IScriptService.h"
#include "System.h"
#include "Core/Systems/CameraSystem.h"
#include "Core/Systems/MovementSystem.h"
#include "Core/Systems/RenderSystem.h"
#include "Core/Systems/SpriteSystem.h"
#include "Core/Xml.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "tinyxml2.h"

namespace Elysium {

// "World2D" (and the older "World") was the flat 2D world, which is now a World3D layer lying on
// the ground: it draws the same, as seen by the default camera.
static SceneLayerSpace ParseSceneLayerSpace(const char* str, bool& ground) {
    const std::string s = str ? str : "World3D";
    if (s == "Screen" || s == "Screen2D") return SceneLayerSpace::Screen2D;
    if (s == "World2D" || s == "World") ground = true;
    return SceneLayerSpace::World3D;
}

static SceneLayerBlend ParseSceneLayerBlend(const char* str) {
    if (!str) return SceneLayerBlend::Normal;
    std::string s = str;
    if (s == "Additive") return SceneLayerBlend::Additive;
    if (s == "Multiply") return SceneLayerBlend::Multiply;
    return SceneLayerBlend::Normal;
}

void SceneLayer::LoadXml(SceneLayer& layer, tinyxml2::XMLElement* el) {
    const char* name = el->Attribute("name");
    layer.name = name ? name : "default";
    layer.zIndex = el->IntAttribute("z", 0);
    layer.opacity = el->FloatAttribute("opacity", 1.0f);
    layer.isVisible = el->BoolAttribute("isVisible", true);
    layer.isComposited = el->BoolAttribute("isComposited", false);
    layer.ground = el->BoolAttribute("ground", false);
    layer.space = ParseSceneLayerSpace(el->Attribute("space"), layer.ground);

    // Support both "blend" (shorthand) and "layerBlend" attributes
    const char* layerBlendAttr = el->Attribute("layerBlend");
    layer.layerBlend = ParseSceneLayerBlend(layerBlendAttr);

    // compositeBlend defaults to layerBlend if not specified
    const char* compositeBlendAttr = el->Attribute("compositeBlend");
    layer.compositeBlend = ParseSceneLayerBlend(compositeBlendAttr);

    if (const char* lightAmbient = el->Attribute("lightAmbient")) layer.lightAmbient = ParseHexColor(lightAmbient, layer.lightAmbient);
    if (const char* sun = el->Attribute("sunColor")) layer.sunColor = ParseHexColor(sun, layer.sunColor);
    layer.sunIntensity = el->FloatAttribute("sunIntensity", layer.sunIntensity);
    layer.sunYaw = el->FloatAttribute("sunYaw", layer.sunYaw);
    layer.sunPitch = el->FloatAttribute("sunPitch", layer.sunPitch);
    layer.rimLight = el->FloatAttribute("rimLight", layer.rimLight);
    layer.shadows = el->BoolAttribute("shadows", layer.shadows);
    layer.shadowBias = el->FloatAttribute("shadowBias", layer.shadowBias);
    layer.fogOfWar = el->FloatAttribute("fogOfWar", layer.fogOfWar);
    if (const char* fog = el->Attribute("fogColor")) layer.fogColor = ParseHexColor(fog, layer.fogColor);

    // Parse ambient color
    const char* ambientStr = el->Attribute("ambient");
    if (ambientStr) {
        layer.ambient = ParseHexColor(ambientStr, {0, 0, 0, 0});
    }
}

Scene::Scene(ServiceLocator& services) : services_(services) {
    world_ = std::make_unique<World>();
}

Scene::~Scene() {
}

void Scene::OnUpdate(float deltaTime, bool isPlaying) {
    for (auto& system : systems_) {
        if (system->IsEnabled() && (isPlaying || system->RunsWhenPaused()))
        {
            ProfileN("System::Update");
            ProfileName(system->GetName().c_str());
            system->Update(deltaTime);
        }
    }

    if (isPlaying && !sceneScriptPath_.empty()) {
        auto& scriptService = services_.Get<Services::IScriptService>();
        scriptService.SetActiveWorld(world_.get());
        if (!isSceneScriptInitialized_) {
            if (scriptService.InitializeScene(Path(sceneScriptPath_))) {
                isSceneScriptInitialized_ = true;
            }
        } else {
            scriptService.UpdateScene(Path(sceneScriptPath_), deltaTime);
            scriptService.RenderScene(Path(sceneScriptPath_));
        }
    }
}

void Scene::OnDraw(Rectangle screen) {
    // RenderSystem now handles all camera rendering internally
    // Just render all systems - no need for manual camera management
    for (auto& system : systems_) {
        if (system->IsVisible()) {
            ProfileN("System::Draw");
            ProfileName(system->GetName().c_str());
            system->Draw();
        }
    }
}

void Scene::OnEvent(Event& event) {
    // Scene script gets first crack at events
    if (!sceneScriptPath_.empty() && isSceneScriptInitialized_) {
        auto& scriptService = services_.Get<Services::IScriptService>();
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

void Scene::CopySetupFrom(const Scene& host, bool pausedSystemsOnly) {
    SetConfiguration(host.GetConfiguration());
    for (const auto& layer : host.GetLayers()) AddLayer(layer);
    for (const auto& hostSystem : host.GetSystems()) {
        if (hostSystem->GetName().empty() || (pausedSystemsOnly && !hostSystem->RunsWhenPaused())) continue;
        Context context{.services = &services_, .scene = this, .world = GetWorld()};
        if (auto system = SystemRegistry::Instance().Create(hostSystem->GetName(), context)) {
            system->Initialize(hostSystem->GetParameters());
            AddSystem(std::move(system));
        }
    }
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

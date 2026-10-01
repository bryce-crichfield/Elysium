#include "Scene.h"
#include <algorithm>
#include <sstream>
#include <string>
#include "Core/Log.h"
#include "Core/ServiceLocator.h"
#include "Entity.h"
#include "Event.h"
#include "Interfaces/IScriptService.h"
#include "System.h"
#include "Systems/CameraSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/SpriteSystem.h"
#include "Core/Xml.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "tinyxml2.h"

namespace Elysium {

static SceneLayerSpace ParseSceneLayerSpace(const char* str) {
    if (!str) return SceneLayerSpace::World2D;
    std::string s = str;
    if (s == "Screen" || s == "Screen2D") return SceneLayerSpace::Screen2D;
    return SceneLayerSpace::World2D;
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
    layer.space = ParseSceneLayerSpace(el->Attribute("space"));

    // Support both "blend" (shorthand) and "layerBlend" attributes
    const char* layerBlendAttr = el->Attribute("layerBlend");
    layer.layerBlend = ParseSceneLayerBlend(layerBlendAttr);

    // compositeBlend defaults to layerBlend if not specified
    const char* compositeBlendAttr = el->Attribute("compositeBlend");
    layer.compositeBlend = ParseSceneLayerBlend(compositeBlendAttr);

    layer.isLit = el->BoolAttribute("lit", false);
    if (const char* lightAmbient = el->Attribute("lightAmbient")) layer.lightAmbient = ParseHexColor(lightAmbient, layer.lightAmbient);
    layer.lightReach = el->FloatAttribute("lightReach", layer.lightReach);
    layer.lightHeight = el->FloatAttribute("lightHeight", layer.lightHeight);
    layer.lightStrength = el->FloatAttribute("lightStrength", layer.lightStrength);
    layer.lightBands = el->IntAttribute("lightBands", 0);
    layer.shadows = el->BoolAttribute("shadows", layer.shadows);
    layer.pointLights = el->BoolAttribute("pointLights", layer.pointLights);
    layer.shadowBias = el->FloatAttribute("shadowBias", layer.shadowBias);
    layer.fogOfWar = el->FloatAttribute("fogOfWar", layer.fogOfWar);
    if (const char* fog = el->Attribute("fogColor")) layer.fogColor = ParseHexColor(fog, layer.fogColor);
    layer.lightDebug = el->IntAttribute("lightDebug", 0);  // read for testing, never saved
    layer.outline = el->FloatAttribute("outline", 0.0f);
    if (const char* outlineColor = el->Attribute("outlineColor")) layer.outlineColor = ParseHexColor(outlineColor, layer.outlineColor);

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
            system->Update(deltaTime);
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
        if (system->IsVisible())
            system->Draw();
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

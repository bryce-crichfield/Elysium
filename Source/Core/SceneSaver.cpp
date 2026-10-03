#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include "Application.h"
#include "Entity.h"
#include "Scene.h"
#include "Core/Log.h"
#include "System.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "Core/PrefabInstance.h"
#include "Core/Components/PrefabInstanceComponent.h"
#include "Core/Components/CameraComponent.h"
#include "Core/Components/FollowComponent.h"
#include "Core/Components/ParentComponent.h"
#include "Core/Components/LayerComponent.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Components/RectangleComponent.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium {

std::string LayerSpaceToString(SceneLayerSpace space) {
    switch (space) {
        case SceneLayerSpace::Screen2D:
            return "Screen2D";
        default:
            return "World3D";
    }
}

std::string LayerBlendToString(SceneLayerBlend blend) {
    switch (blend) {
        case SceneLayerBlend::Normal:
            return "Normal";
        case SceneLayerBlend::Additive:
            return "Additive";
        case SceneLayerBlend::Multiply:
            return "Multiply";
        default:
            return "Normal";
    }
}

void SaveLayers(XMLBuilder& builder, const Scene& scene) {
    const auto& config = scene.GetConfiguration();
    auto configBuilder = builder.AddElement("SceneConfiguration")
        .SetAttribute("width", config.resolutionWidth)
        .SetAttribute("height", config.resolutionHeight);

    for (const auto& layer : scene.GetLayers()) {
        auto layerBuilder = configBuilder.AddElement("SceneLayer")
            .SetAttribute("name", layer.name.c_str())
            .SetAttribute("z", layer.zIndex)
            .SetAttribute("space", LayerSpaceToString(layer.space).c_str())
            .SetAttribute("layerBlend", LayerBlendToString(layer.layerBlend).c_str())
            .SetAttribute("compositeBlend", LayerBlendToString(layer.compositeBlend).c_str())
            .SetAttribute("isComposited", layer.isComposited)
            .SetAttribute("isVisible", layer.isVisible)
            .SetAttribute("opacity", layer.opacity);
        if (layer.ground) layerBuilder.SetAttribute("ground", true);
        if (layer.IsLit()) {
            layerBuilder.SetAttribute("lightAmbient", ColorToHex(layer.lightAmbient).c_str())
                .SetAttribute("sunColor", ColorToHex(layer.sunColor).c_str())
                .SetAttribute("sunIntensity", layer.sunIntensity)
                .SetAttribute("sunYaw", layer.sunYaw)
                .SetAttribute("sunPitch", layer.sunPitch)
                .SetAttribute("rimLight", layer.rimLight)
                .SetAttribute("shadows", layer.shadows)
                .SetAttribute("shadowBias", layer.shadowBias)
                .SetAttribute("fogOfWar", layer.fogOfWar)
                .SetAttribute("fogColor", ColorToHex(layer.fogColor).c_str());
        }
        // Only write ambient if it has a non-zero value
        if (layer.ambient.r != 0 || layer.ambient.g != 0 || layer.ambient.b != 0 || layer.ambient.a != 0) {
            layerBuilder.SetAttribute("ambient", ColorToHex(layer.ambient).c_str());
        }
    }
}

void SaveEntities(XMLBuilder& builder, World* world) {
    auto entitiesBuilder = builder.AddElement("Entities");

    const auto& savers = ComponentRegistry::Instance().GetXmlSavers();

    const auto& entities = world->GetLivingEntities();
    for (Entity entity : entities) {
        // Prefab placements are written by PrefabInstances::Save as <PrefabInstance> blocks.
        if (world->HasComponent<PrefabInstanceComponent>(entity)) continue;

        std::string entityName = world->GetEntityName(entity);

        // Skip layer entities, as they're saved separately
        if (entityName.length() >= 6 && entityName.substr(0, 6) == "Layer_") continue;

        auto entityBuilder = entitiesBuilder.AddElement("Entity")
                                 .SetAttribute("name", entityName.c_str());

        // Dispatch to each registered saver (each checks HasComponent internally)
        for (const auto& [name, saver] : savers) {
            saver(entityBuilder, world, entity);
        }

        // CameraComponent special case: save follow target from ParentComponent
        if (world->HasComponent<CameraComponent>(entity)) {
            auto cameraBuilder = entityBuilder.AddElement("CameraComponent");
            if (world->HasComponent<ParentComponent>(entity)) {
                auto& parentComp = world->GetComponent<ParentComponent>(entity);
                if (!parentComp.targetName.empty()) {
                    cameraBuilder.SetAttribute("target", parentComp.targetName.c_str());
                }
            }
        }
    }
}

void SaveSystems(XMLBuilder& builder, const Scene& scene) {
    auto systemsBuilder = builder.AddElement("Systems");

    const auto& systems = scene.GetSystems();
    for (const auto& system : systems) {
        const std::string& systemName = system->GetName();
        if (systemName.empty()) continue;  // Skip systems not created through the registry
        auto systemBuilder = systemsBuilder.AddElement("System");
        systemBuilder.SetAttribute("type", systemName.c_str());
        // Only parameters changed from their defaults, so scene files stay terse.
        const SystemParameters defaults = system->GetDefaultParameters();
        for (const auto& [name, value] : system->GetParameters()) {
            auto it = defaults.find(name);
            if (it != defaults.end() && it->second == value) continue;
            systemBuilder.SetAttribute(name.c_str(), value.ToString().c_str());
        }
    }
}

bool SaveScene(Scene& scene, const std::string& path) {
    LOG_INFOF("Scene", "Saving scene to XML: %s", path.c_str());
    XMLDocument doc;
    XMLElement* root = doc.NewElement("Scene");
    doc.InsertFirstChild(root);

    XMLBuilder builder(&doc, root);
    World* world = scene.GetWorld();

    if (!scene.GetSceneScript().empty()) {
        builder.AddElement("SceneScript")
            .SetAttribute("path", scene.GetSceneScript().c_str());
    }

    if (!scene.GetEditorMetadata().empty()) {
        auto metadata = builder.AddElement("EditorMetadata");
        for (const auto& [key, value] : scene.GetEditorMetadata()) metadata.SetAttribute(key.c_str(), value.c_str());
    }

    SaveSystems(builder, scene);
    SaveLayers(builder, scene);
    SaveEntities(builder, world);
    PrefabInstances::Save(builder, world, scene.GetServices(), &scene);

    if (!SaveXml(path, doc)) {
        LOG_ERROR("Scene", "Failed to save scene file.");
        return false;
    }

    LOG_INFO("Scene", "Scene saved successfully");
    return true;
}
}  // namespace Elysium

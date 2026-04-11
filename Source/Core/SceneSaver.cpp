#include <sstream>
#include <string>
#include "Application.h"
#include "Entity.h"
#include "Scene.h"
#include "Services/LogService.h"
#include "System.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "Components/CameraComponent.h"
#include "Components/FollowComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/TileComponent.h"
#include "raylib.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium {

std::string LayerSpaceToString(SceneLayerSpace space) {
    switch (space) {
        case SceneLayerSpace::Screen2D:
            return "Screen2D";
        case SceneLayerSpace::World2D:
            return "World2D";
        default:
            return "World2D";
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
        case SceneLayerBlend::Alpha:
            return "Alpha";
        default:
            return "Normal";
    }
}

void SaveLayers(XMLBuilder& builder, const Scene& scene) {
    auto layersBuilder = builder.AddElement("Layers");

    for (const auto& layer : scene.GetLayers()) {
        layersBuilder.AddElement("Layer")
            .SetAttribute("name", layer.name.c_str())
            .SetAttribute("z", layer.zIndex)
            .SetAttribute("space", LayerSpaceToString(layer.space).c_str())
            .SetAttribute("layerBlend", LayerBlendToString(layer.layerBlend).c_str())
            .SetAttribute("compositeBlend", LayerBlendToString(layer.compositeBlend).c_str())
            .SetAttribute("opacity", layer.opacity)
            .SetAttribute("isVisible", layer.isVisible);
    }
}

void SaveTilemap(XMLBuilder& builder, World* world) {
    // Collect all tile entities and their definitions
    struct TileDef {
        RectangleComponent rect;
        std::string layerName;
    };
    std::unordered_map<int, TileDef> tileDefinitions;
    std::vector<int> tilemask;
    int maxX = 0, maxY = 0;

    if (!tileDefinitions.empty()) {
        auto tilemapBuilder = builder.AddElement("Tilemap")
                                  .SetAttribute("width", maxX + 1)
                                  .SetAttribute("height", maxY + 1);

        auto tileDefsBuilder = tilemapBuilder.AddElement("TileDefinitions");
        for (const auto& [id, def] : tileDefinitions) {
            auto tileDefBuilder = tileDefsBuilder.AddElement("TileDefinition")
                                      .SetAttribute("id", id)
                                      .SetAttribute("layerName", def.layerName.c_str());

            std::string bgHex = ColorToHex(def.rect.background);
            std::string borderHex = ColorToHex(def.rect.border);
            if (!bgHex.empty())
                tileDefBuilder.SetAttribute("background", bgHex.c_str());
            if (!borderHex.empty())
                tileDefBuilder.SetAttribute("border", borderHex.c_str());
        }

        auto tilemaskBuilder = tilemapBuilder.AddElement("Tilemask");
        std::ostringstream maskStream;
        for (size_t i = 0; i < tilemask.size(); ++i) {
            if (i > 0)
                maskStream << " ";
            maskStream << tilemask[i];
        }
        tilemaskBuilder.SetText(maskStream.str().c_str());
    }
}

void SaveEntities(XMLBuilder& builder, World* world) {
    auto entitiesBuilder = builder.AddElement("Entities");

    const auto& savers = ComponentRegistry::Instance().GetXmlSavers();

    const auto& entities = world->GetLivingEntities();
    for (Entity entity : entities) {
        std::string entityName = world->GetEntityName(entity);

        // Skip layer entities and tile entities as they're saved separately
        if ((entityName.length() >= 6 && entityName.substr(0, 6) == "Layer_") ||
            world->HasComponent<TileComponent>(entity)) {
            continue;
        }

        auto entityBuilder = entitiesBuilder.AddElement("Entity")
                                 .SetAttribute("name", entityName.c_str());

        // Dispatch to each registered saver (each checks HasComponent internally)
        for (const auto& [name, saver] : savers) {
            saver(entityBuilder, world, entity);
        }

        // CameraComponent special case: needs FollowComponent access
        if (world->HasComponent<CameraComponent>(entity)) {
            auto cameraBuilder = entityBuilder.AddElement("CameraComponent");
            if (world->HasComponent<FollowComponent>(entity)) {
                auto& follow = world->GetComponent<FollowComponent>(entity);
                cameraBuilder.SetAttribute("target", follow.targetEntityName.c_str());
            }
        }
    }
}

void SaveSystems(XMLBuilder& builder, const Scene& scene) {
    auto systemsBuilder = builder.AddElement("Systems");

    const auto& systems = scene.GetSystems();
    for (const auto& system : systems) {
        std::string systemName = system->GetName();
        systemsBuilder.AddElement("System")
            .SetAttribute("type", systemName.c_str());
    }
}

bool SaveScene(Scene& scene, const std::string& path) {
    LOG_INFOF("Scene", "Saving scene to XML: %s", path.c_str());
    XMLDocument doc;
    XMLElement* root = doc.NewElement("Scene");
    doc.InsertFirstChild(root);

    XMLBuilder builder(&doc, root);
    World* world = scene.GetWorld();

    SaveLayers(builder, scene);
    SaveTilemap(builder, world);
    SaveEntities(builder, world);
    SaveSystems(builder, scene);

    if (!SaveXml(path, doc)) {
        LOG_ERROR("Scene", "Failed to save scene file.");
        return false;
    }

    LOG_INFO("Scene", "Scene saved successfully");
    return true;
}
}  // namespace Elysium

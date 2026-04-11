#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include "Application.h"
#include "Entity.h"
#include "Scene.h"
#include "Services/LogService.h"
#include "System.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "Components/CameraComponent.h"
#include "Components/FollowComponent.h"
#include "Components/LayerComponent.h"
#include "Components/PositionComponent.h"
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
        // Only write ambient if it has a non-zero value
        if (layer.ambient.r != 0 || layer.ambient.g != 0 || layer.ambient.b != 0 || layer.ambient.a != 0) {
            layerBuilder.SetAttribute("ambient", ColorToHex(layer.ambient).c_str());
        }
    }
}

void SaveTilemap(XMLBuilder& builder, World* world) {
    struct TileEntry {
        int tileX = 0, tileY = 0;
        Color background = BLANK;
        Color border = BLANK;
        std::string layerName;
    };

    std::vector<TileEntry> tiles;
    float tileWidth = 32.0f, tileHeight = 32.0f;
    bool isIsometric = false;

    world->Query<TileComponent, PositionComponent, RectangleComponent, LayerComponent>(
        [&](Entity, TileComponent& tile, PositionComponent& pos, RectangleComponent& rect, LayerComponent& layer) {
            tileWidth = tile.tileWidth;
            tileHeight = tile.tileHeight;
            isIsometric = tile.isIsometric;

            int tx, ty;
            if (isIsometric) {
                float sum  = 2.0f * pos.y / tileHeight;
                float diff = 2.0f * pos.x / tileWidth;
                tx = (int)std::round((sum + diff) / 2.0f);
                ty = (int)std::round((sum - diff) / 2.0f);
            } else {
                tx = (int)std::round(pos.x / tileWidth);
                ty = (int)std::round(pos.y / tileHeight);
            }

            tiles.push_back({tx, ty, rect.background, rect.border, layer.name});
        });

    if (tiles.empty()) return;

    int gridW = 0, gridH = 0;
    for (const auto& t : tiles) {
        gridW = std::max(gridW, t.tileX + 1);
        gridH = std::max(gridH, t.tileY + 1);
    }

    // Build unique tile definitions ordered by first appearance
    using TileKey = std::tuple<unsigned int, unsigned int, std::string>;
    std::map<TileKey, int> tileDefMap;
    int nextId = 0;
    for (const auto& t : tiles) {
        TileKey key{(unsigned int)ColorToInt(t.background), (unsigned int)ColorToInt(t.border), t.layerName};
        if (tileDefMap.find(key) == tileDefMap.end()) {
            tileDefMap[key] = nextId++;
        }
    }

    // Build tilemask grid
    std::vector<int> tilemask(gridW * gridH, 0);
    for (const auto& t : tiles) {
        TileKey key{(unsigned int)ColorToInt(t.background), (unsigned int)ColorToInt(t.border), t.layerName};
        tilemask[t.tileY * gridW + t.tileX] = tileDefMap[key];
    }

    auto tilemapBuilder = builder.AddElement("Tilemap")
        .SetAttribute("width", gridW)
        .SetAttribute("height", gridH)
        .SetAttribute("isIsometric", isIsometric)
        .SetAttribute("tileWidth", (int)tileWidth)
        .SetAttribute("tileHeight", (int)tileHeight);

    auto tileDefsBuilder = tilemapBuilder.AddElement("TileDefinitions");
    for (const auto& [key, id] : tileDefMap) {
        const auto& [bg, border, layerName] = key;
        auto defBuilder = tileDefsBuilder.AddElement("TileDefinition")
            .SetAttribute("id", id)
            .SetAttribute("layerName", layerName.c_str());
        std::string bgHex = ColorToHex(GetColor(bg));
        std::string borderHex = ColorToHex(GetColor(border));
        if (!bgHex.empty()) defBuilder.SetAttribute("background", bgHex.c_str());
        if (!borderHex.empty()) defBuilder.SetAttribute("border", borderHex.c_str());
    }

    std::ostringstream maskStream;
    for (int y = 0; y < gridH; ++y) {
        if (y > 0) maskStream << "\n\t\t\t";
        for (int x = 0; x < gridW; ++x) {
            if (x > 0) maskStream << " ";
            maskStream << tilemask[y * gridW + x];
        }
    }
    tilemapBuilder.AddElement("Tilemask").SetText(maskStream.str().c_str());
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
        const std::string& systemName = system->GetName();
        if (systemName.empty()) continue;  // Skip systems not created through the registry
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

    if (!scene.GetSceneScript().empty()) {
        builder.AddElement("SceneScript")
            .SetAttribute("path", scene.GetSceneScript().c_str());
    }

    SaveSystems(builder, scene);
    SaveLayers(builder, scene);
    SaveTilemap(builder, world);
    SaveEntities(builder, world);

    if (!SaveXml(path, doc)) {
        LOG_ERROR("Scene", "Failed to save scene file.");
        return false;
    }

    LOG_INFO("Scene", "Scene saved successfully");
    return true;
}
}  // namespace Elysium

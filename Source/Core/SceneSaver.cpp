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
#include "Core/Prefab.h"
#include "Components/CameraComponent.h"
#include "Components/FollowComponent.h"
#include "Components/ParentComponent.h"
#include "Components/LayerComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/TileComponent.h"
#include "Components/PrefabInstanceComponent.h"
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
        std::string tileName;
        std::string variantName;
        std::string layerName;
    };

    std::vector<TileEntry> tiles;
    float tileWidth = 64.0f, tileHeight = 32.0f;
    bool isIsometric = false;

    world->Query<TileComponent, TransformComponent, LayerComponent>(
        [&](Entity, TileComponent& tile, TransformComponent& transform, LayerComponent& layer) {
            tileWidth    = tile.tileWidth;
            tileHeight   = tile.tileHeight;
            isIsometric  = tile.isIsometric;

            int tx, ty;
            if (isIsometric) {
                float sum  = 2.0f * transform.localY / tileHeight;
                float diff = 2.0f * transform.localX / tileWidth;
                tx = (int)std::round((sum + diff) / 2.0f);
                ty = (int)std::round((sum - diff) / 2.0f);
            } else {
                tx = (int)std::round(transform.localX / tileWidth);
                ty = (int)std::round(transform.localY / tileHeight);
            }

            tiles.push_back({tx, ty, tile.tileName, tile.variantName, layer.name});
        });

    if (tiles.empty()) return;

    int gridW = 0, gridH = 0;
    for (const auto& t : tiles) {
        gridW = std::max(gridW, t.tileX + 1);
        gridH = std::max(gridH, t.tileY + 1);
    }

    // Build unique tile definitions ordered by first appearance
    using TileKey = std::tuple<std::string, std::string, std::string>;
    std::map<TileKey, int> tileDefMap;
    int nextId = 0;
    for (const auto& t : tiles) {
        TileKey key{t.tileName, t.variantName, t.layerName};
        if (tileDefMap.find(key) == tileDefMap.end()) {
            tileDefMap[key] = nextId++;
        }
    }

    // Build tilemask grid
    std::vector<int> tilemask(gridW * gridH, 0);
    for (const auto& t : tiles) {
        TileKey key{t.tileName, t.variantName, t.layerName};
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
        const auto& [tileName, variantName, layerName] = key;
        tileDefsBuilder.AddElement("TileDefinition")
            .SetAttribute("id", id)
            .SetAttribute("tile", tileName.c_str())
            .SetAttribute("variant", variantName.c_str())
            .SetAttribute("layer", layerName.c_str());
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
        // Prefab-instance entities are saved via SavePrefabInstances instead, as a
        // <PrefabInstance>/<Override> block rather than a flattened <Entity>.
        if (world->HasComponent<PrefabInstanceComponent>(entity)) {
            continue;
        }

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

// Groups entities carrying PrefabInstanceComponent by instance id, diffs each against a
// freshly-loaded copy of its prefab's current defaults, and emits a
// <PrefabInstance src=... id=...> block per group containing only the fields that
// differ. Anything not listed as an Override is read fresh from the prefab file on the
// next load, which is what makes editing a prefab propagate to its placements.
void SavePrefabInstances(XMLBuilder& builder, World* world, const std::string& basePath) {
    struct InstanceGroup {
        std::string src;
        std::vector<Entity> entities;
    };
    std::map<std::string, InstanceGroup> groups;  // keyed by instanceId, ordered for stable output

    for (Entity entity : world->GetLivingEntities()) {
        if (!world->HasComponent<PrefabInstanceComponent>(entity)) continue;
        const auto& pic = world->GetComponent<PrefabInstanceComponent>(entity);
        auto& group = groups[pic.instanceId];
        group.src = pic.src;
        group.entities.push_back(entity);
    }

    for (const auto& [instanceId, group] : groups) {
        std::string fullPath = basePath + group.src;
        XMLDocument prefabDoc;
        XMLElement* entitiesRoot = nullptr;
        if (!LoadPrefabFile(fullPath, prefabDoc, entitiesRoot)) {
            LOG_ERRORF("Scene", "Skipping save of PrefabInstance '%s': could not reload '%s'",
                       instanceId.c_str(), group.src.c_str());
            continue;
        }

        // Reconstruct defaults with the SAME instanceId as the live entities so
        // namespaced fields (NameComponent, intra-prefab ParentComponent targets)
        // compare as unmodified when the user hasn't actually changed them.
        World defaultWorld;
        PrefabIdMap defaultIdToEntity = LoadPrefabEntities(entitiesRoot, &defaultWorld, instanceId);

        auto instanceBuilder = builder.AddElement("PrefabInstance")
                                   .SetAttribute("src", group.src.c_str())
                                   .SetAttribute("id", instanceId.c_str());

        for (Entity liveEntity : group.entities) {
            const auto& pic = world->GetComponent<PrefabInstanceComponent>(liveEntity);
            auto defaultIt = defaultIdToEntity.find(pic.localEntityId);
            if (defaultIt == defaultIdToEntity.end()) {
                LOG_WARNINGF("Scene",
                             "PrefabInstance '%s': entity id %d no longer exists in '%s'; dropping its overrides.",
                             instanceId.c_str(), pic.localEntityId, group.src.c_str());
                continue;
            }

            auto overrides = DiffPrefabEntity(pic.localEntityId, world, liveEntity, &defaultWorld, defaultIt->second);
            for (const auto& ov : overrides) {
                instanceBuilder.AddElement("Override")
                    .SetAttribute("entity", ov.entity)
                    .SetAttribute("component", ov.component.c_str())
                    .SetAttribute("field", ov.field.c_str())
                    .SetAttribute("value", ov.value.c_str());
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
    SavePrefabInstances(builder, world, GetBasePath(path));

    if (!SaveXml(path, doc)) {
        LOG_ERROR("Scene", "Failed to save scene file.");
        return false;
    }

    LOG_INFO("Scene", "Scene saved successfully");
    return true;
}
}  // namespace Elysium

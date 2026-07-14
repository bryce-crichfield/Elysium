#include <sstream>
#include <string>
#include "Application.h"
#include "Entity.h"
#include "Scene.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "System.h"
#include "Core/Xml.h"
#include "Core/ComponentRegistry.h"
#include "Core/SystemRegistry.h"
#include "Core/Components.h"
#include "Core/Prefab.h"
#include "Systems/SpatialSystem.h"
#include "raylib.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium {

void LoadLayers(XMLElement* root, Scene& scene) {
    VisitElement(root, "SceneConfiguration", [&](XMLElement* configElement) {
        LOG_DEBUG("Scene", "Processing SceneConfiguration section");

        SceneConfiguration config;
        // TODO: Utilize configured scene resolution for render target setup and coordinate calculations
        config.resolutionWidth = configElement->FloatAttribute("width", 640.0f);
        config.resolutionHeight = configElement->FloatAttribute("height", 480.0f);

        ForEachElement(configElement, "SceneLayer", [&](XMLElement* xmlLayer) {
            SceneLayer layer;
            SceneLayer::LoadXml(layer, xmlLayer);
            scene.AddLayer(layer);
            config.layers.push_back(layer);
            LOG_DEBUGF("Scene", "Created scene layer '%s' with z-index %d", layer.name.c_str(), layer.zIndex);
        });

        scene.SetConfiguration(config);
    });
}

void LoadTilemap(XMLElement* root, World* world, float& outTileWidth, float& outTileHeight, bool& outIsIsometric) {
    VisitElement(root, "Tilemap", [&](XMLElement* tilemap) {
        struct TileDef {
            std::string tileName;
            std::string variantName = "default";
            std::string layerName = "tile";
        };
        std::unordered_map<int, TileDef> tileDefinitions;
        std::vector<int> tilemask;
        int tilemapWidth = tilemap->IntAttribute("width", 0);
        float tileWidth = tilemap->FloatAttribute("tileWidth", 32.0f);
        float tileHeight = tilemap->FloatAttribute("tileHeight", 32.0f);
        bool isIsometric = tilemap->BoolAttribute("isIsometric", false);

        outTileWidth = tileWidth;
        outTileHeight = tileHeight;
        outIsIsometric = isIsometric;

        VisitElement(tilemap, "Tilemask", [&](XMLElement* xmlTileMask) {
            const char* textContent = xmlTileMask->GetText();
            if (textContent) {
                std::string content(textContent);
                size_t start = content.find_first_not_of(" \t\n\r");
                size_t end = content.find_last_not_of(" \t\n\r");
                if (start != std::string::npos && end != std::string::npos) {
                    content = content.substr(start, end - start + 1);
                    std::istringstream iss(content);
                    std::string token;
                    while (iss >> token) {
                        tilemask.push_back(std::stoi(token));
                    }
                }
            }
        });

        VisitElement(tilemap, "TileDefinitions", [&](XMLElement* xmlTileDefinitions) {
            ForEachElement(xmlTileDefinitions, "TileDefinition", [&](XMLElement* xmlTileDefinition) {
                int id = xmlTileDefinition->IntAttribute("id", 0);
                TileDef def;
                def.tileName    = xmlTileDefinition->Attribute("tile")    ? xmlTileDefinition->Attribute("tile")    : "";
                def.variantName = xmlTileDefinition->Attribute("variant") ? xmlTileDefinition->Attribute("variant") : "default";
                def.layerName   = xmlTileDefinition->Attribute("layer")   ? xmlTileDefinition->Attribute("layer")   : "tile";
                tileDefinitions[id] = std::move(def);
            });
        });

        // Create a container entity for all tiles
        Entity tilemapParent = world->CreateEntity();
        world->AddComponent<NameComponent>(tilemapParent, NameComponent("Tilemap"));
        world->AddComponent<TransformComponent>(tilemapParent, TransformComponent(0, 0));

        for (size_t i = 0; i < tilemask.size(); i++) {
            int id = tilemask[i];
            auto defIt = tileDefinitions.find(id);
            if (defIt == tileDefinitions.end()) continue;

            int tileX = (int)(i % tilemapWidth);
            int tileY = (int)(i / tilemapWidth);
            int worldX = isIsometric ? (tileX - tileY) * (int)(tileWidth  / 2) : tileX * (int)tileWidth;
            int worldY = isIsometric ? (tileX + tileY) * (int)(tileHeight / 2) : tileY * (int)tileHeight;

            const TileDef& def = defIt->second;

            auto entity = world->CreateEntity();
            world->AddComponent<TransformComponent>(entity, TransformComponent(worldX, worldY));
            world->AddComponent<NameComponent>(entity, NameComponent(std::string("Tile_") + std::to_string(i)));
            world->AddComponent<LayerComponent>(entity, LayerComponent(def.layerName));
            world->AddComponent<TileComponent>(entity, TileComponent(def.tileName, def.variantName, isIsometric, tileWidth, tileHeight));
            world->AddComponent<ParentComponent>(entity, ParentComponent(tilemapParent, "Tilemap"));
            world->AddChild(tilemapParent, entity);
        }
    });
}

void LoadEntities(XMLElement* root, World* world) {
    // Load hand-authored entities
    LOG_INFO("Scene", "Starting entity loading");

    ForEachElement(root, "Entities", [&](XMLElement* entities) {
        LOG_DEBUG("Scene", "Processing Entities section");
        ForEachElement(entities, "Entity", [&](XMLElement* xmlEntity) {
            Entity entity = world->CreateEntity();
            LOG_DEBUGF("Scene", "Created entity: %zu", entity);
            LoadEntityComponents(xmlEntity, world, entity);
        });
    });
}

// Loads every <PrefabInstance src="..." id="..."> under `root`, spawning the referenced
// prefab's entities into `world`, tagging them with PrefabInstanceComponent, and applying
// the instance's <Override> list on top.
void LoadPrefabInstances(XMLElement* root, World* world, const std::string& basePath) {
    ForEachElement(root, "PrefabInstance", [&](XMLElement* xmlInstance) {
        const char* src = xmlInstance->Attribute("src");
        if (!src) {
            LOG_ERROR("Scene", "PrefabInstance missing required 'src' attribute; skipping.");
            return;
        }
        const char* instanceIdAttr = xmlInstance->Attribute("id");
        if (!instanceIdAttr) {
            LOG_ERRORF("Scene", "PrefabInstance for '%s' missing required 'id' attribute; skipping.", src);
            return;
        }
        std::string instanceId = instanceIdAttr;

        std::string fullPath = basePath + src;
        XMLDocument prefabDoc;
        XMLElement* entitiesRoot = nullptr;
        if (!LoadPrefabFile(fullPath, prefabDoc, entitiesRoot)) {
            return;
        }

        PrefabIdMap idToEntity = LoadPrefabEntities(entitiesRoot, world, instanceId);

        for (const auto& [localId, entity] : idToEntity) {
            PrefabInstanceComponent instanceComp;
            instanceComp.src = src;
            instanceComp.instanceId = instanceId;
            instanceComp.localEntityId = localId;
            world->AddComponent<PrefabInstanceComponent>(entity, instanceComp);
        }

        ForEachElement(xmlInstance, "Override", [&](XMLElement* xmlOverride) {
            int localId = xmlOverride->IntAttribute("entity", -1);
            const char* component = xmlOverride->Attribute("component");
            const char* field = xmlOverride->Attribute("field");
            const char* value = xmlOverride->Attribute("value");
            if (!component || !field || !value) {
                LOG_WARNINGF("Scene", "Malformed Override in PrefabInstance '%s'; skipping.", instanceId.c_str());
                return;
            }
            ApplyPrefabOverride(world, idToEntity, localId, component, field, value);
        });
    });
}

// After all entities are spawned, walk every ParentComponent.targetName,
// resolve it to an Entity ID, and populate the World's hierarchy adjacency.
void ResolveHierarchy(World* world) {
    world->Query<ParentComponent>([&](Entity child, ParentComponent& pc) {
        if (pc.targetName.empty()) return;
        // Already resolved directly (e.g. an intra-prefab link resolved by local id in
        // LoadPrefabEntities) — skip the by-name lookup so it doesn't log a spurious
        // "could not resolve" warning for a targetName that was never meant to be a name.
        if (pc.parent != INVALID_ENTITY) return;
        Entity parent = INVALID_ENTITY;
        if (world->GetEntityByName(pc.targetName, &parent)) {
            world->AddChild(parent, child);
        } else {
            LOG_WARNINGF("Scene", "Hierarchy: could not resolve parent name '%s' for entity %zu",
                         pc.targetName.c_str(), child);
        }
    });
}

void LoadSystems(XMLElement* root, Scene& scene) {
    VisitElement(root, "Systems", [&](XMLElement* systemsElement) {
        ForEachElement(systemsElement, "System", [&](XMLElement* xmlSystem) {
            const char* systemType = xmlSystem->Attribute("type");
            if (!systemType)
                return;

            std::string systemName = systemType;

            Context context =
                Context{.application = &Application::GetInstance(), .scene = &scene, .world = scene.GetWorld()};

            auto system = SystemRegistry::Instance().Create(systemName, context);
            if (system) {
                scene.AddSystem(std::move(system));
            }
        });
    });
}

bool LoadScene(Scene& scene, const std::string& path) {
    LOG_INFOF("Scene", "Loading scene from XML: %s", path.c_str());
    XMLDocument doc;

    if (!LoadXml(path, doc)) {
        LOG_ERROR("Scene", "Failed to load scene file.");
        return false;
    }

    XMLElement* root = doc.FirstChildElement("Scene");
    if (!root) {
        LOG_ERROR("Scene", "Invalid scene file format. Missing root tag <Scene>.");
        return false;
    }

    World* world_ = scene.GetWorld();

    // Tilemap properties for position calculation
    float tileWidth = TILE_WIDTH;
    float tileHeight = TILE_HEIGHT;
    bool isIsometric = false;

    LoadLayers(root, scene);
    LoadTilemap(root, world_, tileWidth, tileHeight, isIsometric);
    LoadEntities(root, world_);
    LoadPrefabInstances(root, world_, GetBasePath(path));
    ResolveHierarchy(world_);
    LoadSystems(root, scene);

    // Wire tilemap dimensions into SpatialSystem now that it exists.
    // We also need the tilemap width/height in grid cells, not world units.
    {
        int tilemapGridW = 0, tilemapGridH = 0;
        VisitElement(root, "Tilemap", [&](XMLElement* tilemap) {
            tilemapGridW = tilemap->IntAttribute("width",  0);
            tilemapGridH = tilemap->IntAttribute("height", 0);
        });
        if (tilemapGridW > 0 && tilemapGridH > 0) {
            if (auto* spatial = scene.GetSystem<Elysium::Systems::SpatialSystem>()) {
                spatial->BuildGrid(tilemapGridW, tilemapGridH, tileWidth, tileHeight, isIsometric);
            }
        }
    }

    VisitElement(root, "SceneScript", [&](XMLElement* el) {
        const char* path = el->Attribute("path");
        if (path) {
            scene.SetSceneScript(path);
            auto& assetService = Application::GetInstance().GetService<Services::AssetService>();
            assetService.LoadAsset(AssetType::SCRIPT, Path(path));
        }
    });

    // Allow subclasses to create additional custom systems
    scene.CreateCustomSystems();

    return true;
}
}  // namespace Elysium

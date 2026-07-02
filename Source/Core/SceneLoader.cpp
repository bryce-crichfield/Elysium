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
        world->AddComponent<PositionComponent>(tilemapParent, PositionComponent(0, 0));

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
            world->AddComponent<PositionComponent>(entity, PositionComponent(worldX, worldY));
            world->AddComponent<NameComponent>(entity, NameComponent(std::string("Tile_") + std::to_string(i)));
            world->AddComponent<LayerComponent>(entity, LayerComponent(def.layerName));
            world->AddComponent<TileComponent>(entity, TileComponent(def.tileName, def.variantName, isIsometric, tileWidth, tileHeight));
            world->AddComponent<ParentComponent>(entity, ParentComponent(tilemapParent, "Tilemap"));
            world->AddChild(tilemapParent, entity);
        }
    });
}

// Create map of tag name to component parser functions
using ComponentLoader = std::function<void(XMLElement*, World*, Entity)>;
const std::unordered_map<std::string, ComponentLoader>& ComponentLoaders() {
    static std::unordered_map<std::string, ComponentLoader> componentLoaders;

    if (!componentLoaders.empty())
        return componentLoaders;

    // Load from registry
    const auto& registryLoaders = ComponentRegistry::Instance().GetXmlLoaders();
    for(const auto& [name, loader] : registryLoaders) {
        componentLoaders[name] = loader;
    }
    
    // Register custom overrides
    componentLoaders["CameraComponent"] = [](XMLElement* xmlComponent, World* world, Entity entity) {
        CameraComponent cam{};
        CameraComponent::LoadXml(cam, xmlComponent);
        world->AddComponent(entity, cam);

        // If a follow target is specified, add FollowComponent + ParentComponent so
        // FollowSystem will move the camera toward that target.
        std::string target = xmlComponent->Attribute("target") ? xmlComponent->Attribute("target") : "";
        if (!target.empty()) {
            world->AddComponent(entity, FollowComponent{});  // speed=0 → instant by default
            ParentComponent parentComp;
            parentComp.targetName = target;
            world->AddComponent(entity, parentComp);
        }
    };

    return componentLoaders;
}

void LoadEntities(XMLElement* root, World* world) {
    // Load entities
    LOG_INFO("Scene", "Starting entity loading");

    // ForEachElement handles multiple <Entities> blocks — the original inline block
    // plus any injected by <Include> tags (each prefab file has <Entities> as its root).
    ForEachElement(root, "Entities", [&](XMLElement* entities) {
        LOG_DEBUG("Scene", "Processing Entities section");
        ForEachElement(entities, "Entity", [&](XMLElement* xmlEntity) {
            // Create the entity
            Entity entity = world->CreateEntity();
            LOG_DEBUGF("Scene", "Created entity: %zu", entity);

            // Create the components
            ForEachChild(xmlEntity, [&](XMLElement* component) {
                std::string componentType = component->Name();
                auto parser = ComponentLoaders().find(componentType);
                if (parser != ComponentLoaders().end()) {
                    LOG_DEBUGF("Scene", "Processing component: %s", componentType.c_str());
                    parser->second(component, world, entity);

                    // Backward compatibility: if a component has a layerName attribute, 
                    // add a LayerComponent to the entity if it doesn't have one.
                    const char* layerName = component->Attribute("layerName");
                    if (layerName && !world->HasComponent<LayerComponent>(entity)) {
                        world->AddComponent<LayerComponent>(entity, LayerComponent(layerName));
                    }
                } else {
                    LOG_WARNINGF("Scene", "Unknown component type: %s", componentType.c_str());
                }
            });
        });
    });
}

// After all entities are spawned, walk every ParentComponent.targetName,
// resolve it to an Entity ID, and populate the World's hierarchy adjacency.
void ResolveHierarchy(World* world) {
    world->Query<ParentComponent>([&](Entity child, ParentComponent& pc) {
        if (pc.targetName.empty()) return;
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

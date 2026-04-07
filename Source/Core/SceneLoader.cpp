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
#include "raylib.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium {

void LoadLayers(XMLElement* root, Scene& scene) {
    VisitElement(root, "SceneConfiguration", [&](XMLElement* configElement) {
        LOG_DEBUG("Scene", "Processing SceneConfiguration section");

        SceneConfiguration config;
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
            RectangleComponent rect;
            std::string layerName;
        };
        std::unordered_map<int, TileDef> tileDefinitions;
        std::vector<int> tilemask;
        int tilemapWidth = tilemap->IntAttribute("width", 0);
        float tileWidth = tilemap->FloatAttribute("tileWidth", 32.0f);
        float tileHeight = tilemap->FloatAttribute("tileHeight", 32.0f);
        bool isIsometric = tilemap->BoolAttribute("isIsometric", false);

        // Output tilemap properties for position calculation
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
                std::string bgHex = xmlTileDefinition->Attribute("background") ? xmlTileDefinition->Attribute("background") : "";
                std::string borderHex = xmlTileDefinition->Attribute("border") ? xmlTileDefinition->Attribute("border") : "";
                const char* layerName = xmlTileDefinition->Attribute("layerName");
                Color bgColor = ParseHexColor(bgHex, BLANK);
                Color borderColor = ParseHexColor(borderHex, BLANK);
                tileDefinitions[id] = {RectangleComponent(tileWidth, tileHeight, bgColor, borderColor), layerName ? layerName : "tile"};
            });
        });

        for (size_t i = 0; i < tilemask.size(); i++) {
            int id = tilemask[i];
            int tileX = i % tilemapWidth;
            int tileY = i / tilemapWidth;
            int worldX = isIsometric ? (tileX - tileY) * (tileWidth / 2) : tileX * tileWidth;
            int worldY = isIsometric ? (tileX + tileY) * (tileHeight / 2) : tileY * tileHeight;
            auto entity = world->CreateEntity();
            world->AddComponent<PositionComponent>(entity, PositionComponent(worldX, worldY));
            world->AddComponent<NameComponent>(entity, NameComponent(std::string("Tile_") + std::to_string(i)));
            world->AddComponent<RectangleComponent>(entity, tileDefinitions[id].rect);
            world->AddComponent<LayerComponent>(entity, LayerComponent(tileDefinitions[id].layerName));
            world->AddComponent<TileComponent>(entity, TileComponent(tileWidth, tileHeight, isIsometric));
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
        // Use default loader first
        CameraComponent cam{};
        CameraComponent::LoadXml(cam, xmlComponent);
        world->AddComponent(entity, cam);

        // Custom logic: Add FollowComponent if target is specified
        std::string target = xmlComponent->Attribute("target") ? xmlComponent->Attribute("target") : "";
        if (!target.empty()) {
            FollowComponent followComp;
            followComp.targetEntityName = target;
            world->AddComponent(entity, FollowComponent{1.0f, target});
        }
    };

    return componentLoaders;
}

void LoadEntities(XMLElement* root, World* world) {
    // Load entities
    LOG_INFO("Scene", "Starting entity loading");

    VisitElement(root, "Entities", [&](XMLElement* entities) {
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
    LoadSystems(root, scene);

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

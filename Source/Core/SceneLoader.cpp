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
#include "Core/Components.h"
#include "raylib.h"
#include "tinyxml2.h"

// System includes still needed for LoadSystems
#include "Systems/AnimationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/CommandSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/PickSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/ScriptSystem.h"
#include "Systems/SpriteSystem.h"
#include "Systems/SpatialSystem.h"
#include "Systems/PathfindingSystem.h"
#include "Systems/KinematicsSystem.h"
#include "Systems/AttackSystem.h"
#include "Systems/ProjectileSystem.h"

using namespace tinyxml2;

namespace Elysium {

LayerComponent::Type ParseLayerType(const char* typeStr) {
    if (!typeStr) return LayerComponent::Type::World;
    std::string type = typeStr;
    if (type == "Background") return LayerComponent::Type::Background;
    else if (type == "Lighting") return LayerComponent::Type::Lighting;
    else if (type == "Overlay") return LayerComponent::Type::Overlay;
    return LayerComponent::Type::World;
}

LayerComponent::Space ParseLayerSpace(const char* spaceStr) {
    if (!spaceStr) return LayerComponent::Space::World;
    std::string space = spaceStr;
    if (space == "Screen") return LayerComponent::Space::Screen;
    else if (space == "Parallax") return LayerComponent::Space::Parallax;
    return LayerComponent::Space::World;
}

LayerComponent::Blend ParseLayerBlend(const char* blendStr) {
    if (!blendStr) return LayerComponent::Blend::Normal;
    std::string blend = blendStr;
    if (blend == "Additive") return LayerComponent::Blend::Additive;
    else if (blend == "Multiply") return LayerComponent::Blend::Multiply;
    else if (blend == "Alpha") return LayerComponent::Blend::Alpha;
    return LayerComponent::Blend::Normal;
}

LayerComponent CreateLayerComponent(XMLElement* xmlLayer) {
    LayerComponent layerComp;
    const char* name = xmlLayer->Attribute("name");
    layerComp.name = name ? name : "default";
    layerComp.zIndex = xmlLayer->IntAttribute("z", 0);
    layerComp.opacity = xmlLayer->FloatAttribute("opacity", 1.0f);
    layerComp.isVisible = xmlLayer->BoolAttribute("isVisible", true);
    layerComp.type = ParseLayerType(xmlLayer->Attribute("type"));
    layerComp.space = ParseLayerSpace(xmlLayer->Attribute("space"));
    layerComp.blend = ParseLayerBlend(xmlLayer->Attribute("blend"));
    return layerComp;
}

void LoadLayers(XMLElement* root, World* world) {
    VisitElement(root, "Layers", [&](XMLElement* layersElement) {
        LOG_DEBUG("Scene", "Processing Layers section");
        ForEachElement(layersElement, "Layer", [&](XMLElement* xmlLayer) {
            LayerComponent layerComp = CreateLayerComponent(xmlLayer);
            Entity layerEntity = world->CreateEntity();
            world->AddComponent<NameComponent>(layerEntity, NameComponent(std::string("Layer_") + layerComp.name));
            world->AddComponent(layerEntity, layerComp);
            LOG_DEBUGF("Scene", "Created layer '%s' with z-index %d", layerComp.name.c_str(), layerComp.zIndex);
        });
    });
}

void LoadTilemap(XMLElement* root, World* world) {
    VisitElement(root, "Tilemap", [&](XMLElement* tilemap) {
        std::unordered_map<int, RectangleComponent> tileDefinitions;
        std::vector<int> tilemask;
        int tilemapWidth = tilemap->IntAttribute("width", 0);

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
                tileDefinitions[id] = RectangleComponent(32, 32, bgColor, borderColor, layerName ? layerName : "tile");
            });
        });

        for (size_t i = 0; i < tilemask.size(); i++) {
            int id = tilemask[i];
            int x = i % tilemapWidth;
            int y = i / tilemapWidth;
            auto entity = world->CreateEntity();
            world->AddComponent<NameComponent>(entity, NameComponent(std::string("Tile_") + std::to_string(i)));
            world->AddComponent<LocationComponent>(entity, LocationComponent(x, y));
            world->AddComponent<RectangleComponent>(entity, tileDefinitions[id]);
            world->AddComponent<TileComponent>(entity, TileComponent());
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

            if (systemName == "RenderSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::RenderSystem>(context));
            } else if (systemName == "MovementSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::MovementSystem>(context));
            } else if (systemName == "AnimationSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::AnimationSystem>(context));
            } else if (systemName == "CameraSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::CameraSystem>(context));
            } else if (systemName == "SpriteSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::SpriteSystem>(context));
            } else if (systemName == "PickSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::PickSystem>(context));
            } else if (systemName == "ScriptSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::ScriptSystem>(context));
            } else if (systemName == "CommandSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::CommandSystem>(context));
            } else if (systemName == "SpatialSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::SpatialSystem>(context));
            } else if (systemName == "PathfindingSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::PathfindingSystem>(context));
            } else if (systemName == "KinematicsSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::KinematicsSystem>(context));
            } else if (systemName == "AttackSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::AttackSystem>(context));
            } else if (systemName == "ProjectileSystem") {
                scene.AddSystem(std::make_unique<Elysium::Systems::ProjectileSystem>(context));
            } else {
                LOG_WARNINGF("Scene", "Unknown system type: %s", systemName.c_str());
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

    LoadLayers(root, world_);
    LoadTilemap(root, world_);
    LoadEntities(root, world_);
    LoadSystems(root, scene);

    // We allow the user to define the location and have the position component be implicit
    // Position component represents the CENTER of the tile in world coordinates
    world_->Query<LocationComponent>([&](Entity entity, auto& loc) {
        // Convert tile coordinates to centered world coordinates
        float worldX = loc.x * TILE_WIDTH + TILE_WIDTH * 0.5f;
        float worldY = loc.y * TILE_HEIGHT + TILE_HEIGHT * 0.5f;

        if (!world_->HasComponent<PositionComponent>(entity)) {
            world_->AddComponent<PositionComponent>(entity, PositionComponent(worldX, worldY));
            return;
        }

        auto& pos = world_->GetComponent<PositionComponent>(entity);
        pos.x = worldX;
        pos.y = worldY;
    });

    // Allow subclasses to create additional custom systems
    scene.CreateCustomSystems();

    return true;
}
}  // namespace Elysium

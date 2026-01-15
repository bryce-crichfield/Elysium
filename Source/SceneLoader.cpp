#include "Application.h"
#include "Entity.h"
#include "Scene.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "System.h"
#include "Systems/AnimationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/SpriteSystem.h"
#include "Systems/PickSystem.h"
#include "Xml.h"
#include "raylib.h"
#include "tinyxml2.h"
#include <sstream>
#include <string>

using namespace tinyxml2;

namespace Elysium
{

LayerComponent::Type ParseLayerType(const char *typeStr)
{
    if (!typeStr)
        return LayerComponent::Type::World;
    std::string type = typeStr;
    if (type == "Background")
        return LayerComponent::Type::Background;
    else if (type == "World")
        return LayerComponent::Type::World;
    else if (type == "Lighting")
        return LayerComponent::Type::Lighting;
    else if (type == "Overlay")
        return LayerComponent::Type::Overlay;
    else
        return LayerComponent::Type::World;
}

LayerComponent::Space ParseLayerSpace(const char *spaceStr)
{
    if (!spaceStr)
        return LayerComponent::Space::World;
    std::string space = spaceStr;
    if (space == "Screen")
        return LayerComponent::Space::Screen;
    else if (space == "World")
        return LayerComponent::Space::World;
    else if (space == "Parallax")
        return LayerComponent::Space::Parallax;
    else
        return LayerComponent::Space::World;
}

LayerComponent::Blend ParseLayerBlend(const char *blendStr)
{
    if (!blendStr)
        return LayerComponent::Blend::Normal;
    std::string blend = blendStr;
    if (blend == "Normal")
        return LayerComponent::Blend::Normal;
    else if (blend == "Additive")
        return LayerComponent::Blend::Additive;
    else if (blend == "Multiply")
        return LayerComponent::Blend::Multiply;
    else if (blend == "Alpha")
        return LayerComponent::Blend::Alpha;
    else
        return LayerComponent::Blend::Normal;
}

LayerComponent CreateLayerComponent(XMLElement *xmlLayer)
{
    LayerComponent layerComp;

    const char *name = xmlLayer->Attribute("name");
    layerComp.name = name ? name : "default";
    layerComp.zIndex = xmlLayer->IntAttribute("z", 0);
    layerComp.opacity = xmlLayer->FloatAttribute("opacity", 1.0f);
    layerComp.isVisible = xmlLayer->BoolAttribute("isVisible", true);

    layerComp.type = ParseLayerType(xmlLayer->Attribute("type"));
    layerComp.space = ParseLayerSpace(xmlLayer->Attribute("space"));
    layerComp.blend = ParseLayerBlend(xmlLayer->Attribute("blend"));

    return layerComp;
}

void LoadLayers(XMLElement *root, World *world)
{
    VisitElement(root, "Layers", [&](XMLElement *layersElement) {
        LOG_DEBUG("Scene", "Processing Layers section");
        ForEachElement(layersElement, "Layer", [&](XMLElement *xmlLayer) {
            LayerComponent layerComp = CreateLayerComponent(xmlLayer);
            Entity layerEntity = world->CreateEntity();
            world->AddComponent<NameComponent>(layerEntity, NameComponent(std::string("Layer_") + layerComp.name));
            world->AddComponent(layerEntity, layerComp);
            LOG_DEBUGF("Scene", "Created layer '%s' with z-index %d", layerComp.name.c_str(), layerComp.zIndex);
        });
    });
}

void LoadTilemap(XMLElement *root, World *world)
{
    VisitElement(root, "Tilemap", [&](XMLElement *tilemap) {
        std::unordered_map<int, RectangleComponent> tileDefinitions;
        std::vector<int> tilemask;

        int tilemapWidth = tilemap->IntAttribute("width", 0);
        int tilemapHeight = tilemap->IntAttribute("height", 0);

        VisitElement(tilemap, "Tilemask", [&](XMLElement *xmlTileMask) {
            const char *textContent = xmlTileMask->GetText();
            if (textContent)
            {
                std::string content = std::string(textContent);

                // Remove leading and trailing whitespace
                size_t start = content.find_first_not_of(" \t\n\r");
                size_t end = content.find_last_not_of(" \t\n\r");

                if (start != std::string::npos && end != std::string::npos)
                {
                    content = content.substr(start, end - start + 1);

                    // Split by whitespace and convert to integers
                    std::istringstream iss(content);
                    std::string token;
                    while (iss >> token)
                    {
                        try
                        {
                            int value = std::stoi(token);
                            tilemask.push_back(value);
                        }
                        catch (const std::exception &e)
                        {
                            throw std::exception(e);
                        }
                    }
                }
            }
        });

        VisitElement(tilemap, "TileDefinitions", [&](XMLElement *xmlTileDefinitions) {
            LOG_DEBUG("Scene", "Processing TileDefinitions");
            ForEachElement(xmlTileDefinitions, "TileDefinition", [&](XMLElement *xmlTileDefinition) {
                int id = xmlTileDefinition->IntAttribute("id", 0);
                std::string backgroundHex =
                    xmlTileDefinition->Attribute("background") ? xmlTileDefinition->Attribute("background") : "";
                std::string borderHex =
                    xmlTileDefinition->Attribute("border") ? xmlTileDefinition->Attribute("border") : "";
                const char *layerName = xmlTileDefinition->Attribute("layerName");
                Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                Color borderColor = ParseHexColor(borderHex, BLANK);

                tileDefinitions[id] =
                    RectangleComponent(32, 32, backgroundColor, borderColor, layerName ? layerName : "tile");
                LOG_DEBUGF("Scene", "Created tile definition %d with layer '%s'", id, layerName ? layerName : "tile");
            });
        });

        for (int i = 0; i < tilemask.size(); i++)
        {
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
using ComponentLoader = std::function<void(XMLElement *, World *, Entity)>;
const std::unordered_map<std::string, ComponentLoader> &ComponentLoaders()
{

    static std::unordered_map<std::string, ComponentLoader> componentLoaders;

    if (!componentLoaders.empty())
        return componentLoaders;

    // Register component parser functions
    componentLoaders["NameComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        const char *name = xmlComponent->Attribute("name");
        world->AddComponent(entity, NameComponent(name));
    };

    componentLoaders["PositionComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        float x = xmlComponent->FloatAttribute("x", 0.0f);
        float y = xmlComponent->FloatAttribute("y", 0.0f);
        world->AddComponent(entity, PositionComponent(x, y));
    };

    componentLoaders["LocationComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        int x = xmlComponent->IntAttribute("x", 0);
        int y = xmlComponent->IntAttribute("y", 0);
        world->AddComponent(entity, LocationComponent(x, y));
    };

    componentLoaders["MovementComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        std::vector<Vector2> waypoints;
        VisitElement(xmlComponent, "Waypoints", [&](XMLElement *xmlWaypoints) {
            ForEachElement(xmlWaypoints, "Waypoint", [&](XMLElement *xmlWaypoint) {
                int x = xmlWaypoint->IntAttribute("x", 0);
                int y = xmlWaypoint->IntAttribute("y", 0);
                waypoints.push_back({(float)x, (float)y});
            });
        });
        world->AddComponent(entity, MovementComponent(waypoints));
    };

    componentLoaders["AnimationComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        world->AddComponent(entity, AnimationComponent());
    };

    componentLoaders["DirectionComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        world->AddComponent(entity, DirectionComponent());
    };

    componentLoaders["LayerComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        world->AddComponent(entity, CreateLayerComponent(xmlComponent));
    };

    componentLoaders["RectangleComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        float width = xmlComponent->FloatAttribute("width", 100.0f);
        float height = xmlComponent->FloatAttribute("height", 100.0f);
        std::string backgroundHex = xmlComponent->Attribute("background") ? xmlComponent->Attribute("background") : "";
        std::string borderHex = xmlComponent->Attribute("border") ? xmlComponent->Attribute("border") : "";
        const char *layerName = xmlComponent->Attribute("layerName");

        Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
        Color borderColor = ParseHexColor(borderHex, BLANK);

        world->AddComponent(
            entity, RectangleComponent(width, height, backgroundColor, borderColor, layerName ? layerName : "tile"));
    };

    componentLoaders["CircleComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        float radius = xmlComponent->FloatAttribute("radius", 50.0f);
        std::string fillHex = xmlComponent->Attribute("fill") ? xmlComponent->Attribute("fill") : "";
        std::string borderHex = xmlComponent->Attribute("border") ? xmlComponent->Attribute("border") : "";
        const char *layerName = xmlComponent->Attribute("layerName");

        Color fillColor = ParseHexColor(fillHex, BLANK);
        Color borderColor = ParseHexColor(borderHex, BLANK);

        world->AddComponent(entity, CircleComponent(radius, fillColor, borderColor, layerName ? layerName : "tile"));
    };

    componentLoaders["TextComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        std::string text = xmlComponent->Attribute("text") ? xmlComponent->Attribute("text") : "";
        int fontSize = xmlComponent->IntAttribute("fontSize", 12);
        world->AddComponent(entity, TextComponent(text, fontSize));
    };

    componentLoaders["SpriteComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        const char *spriteName = xmlComponent->Attribute("spriteName");
        const char *markerName = xmlComponent->Attribute("markerName");
        const char *layerName = xmlComponent->Attribute("layerName");

        if (spriteName && markerName)
        {
            auto &assetService = Application::GetInstance().GetService<Elysium::Services::AssetService>();
            Sprite sprite = assetService.GetSprite(spriteName);
            world->AddComponent(entity, SpriteComponent(sprite, markerName, layerName ? layerName : "default"));
        }
        else
        {
            LOG_WARNING("Scene", "SpriteComponent missing required attributes: spriteName or markerName");
        }
    };

    componentLoaders["CameraComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        std::string target = xmlComponent->Attribute("target") ? xmlComponent->Attribute("target") : "";
        world->AddComponent(entity, CameraComponent());

        if (!target.empty())
        {
            FollowComponent followComp;
            followComp.targetEntityName = target;
            world->AddComponent(entity, followComp);
        }
    };

    componentLoaders["LightComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        float radius = xmlComponent->FloatAttribute("radius", 50.0f);
        int r = xmlComponent->IntAttribute("r", 255);
        int g = xmlComponent->IntAttribute("g", 255);
        int b = xmlComponent->IntAttribute("b", 255);
        int a = xmlComponent->IntAttribute("a", 100);
        const char *layerName = xmlComponent->Attribute("layerName");
        Color color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
        world->AddComponent(entity, LightComponent(color, radius, layerName ? layerName : "default"));
    };

    componentLoaders["TileComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        world->AddComponent(entity, TileComponent());
    };

    componentLoaders["TeamComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        int teamId = xmlComponent->IntAttribute("teamId", 0);
        world->AddComponent(entity, TeamComponent(teamId));
    };

    componentLoaders["BoundsComponent"] = [](XMLElement *xmlComponent, World *world, Entity entity) {
        // BoundsComponent is computed by RenderSystem, but we create it here with default values
        world->AddComponent(entity, BoundsComponent());
    };

    return componentLoaders;
}

void LoadEntities(XMLElement *root, World *world)
{
    // Load entities
    LOG_INFO("Scene", "Starting entity loading");

    VisitElement(root, "Entities", [&](XMLElement *entities) {
        LOG_DEBUG("Scene", "Processing Entities section");
        ForEachElement(entities, "Entity", [&](XMLElement *xmlEntity) {
            // Create the entity
            Entity entity = world->CreateEntity();
            LOG_DEBUGF("Scene", "Created entity: %d", entity);

            // Create the components
            ForEachChild(xmlEntity, [&](XMLElement *component) {
                std::string componentType = component->Name();
                auto parser = ComponentLoaders().find(componentType);
                if (parser != ComponentLoaders().end())
                {
                    LOG_DEBUGF("Scene", "Processing component: %s", componentType.c_str());
                    parser->second(component, world, entity);
                }
                else
                {
                    LOG_WARNINGF("Scene", "Unknown component type: %s", componentType.c_str());
                }
            });
        });
    });
}

void LoadSystems(XMLElement *root, Scene &scene)
{
    VisitElement(root, "Systems", [&](XMLElement *systemsElement) {
        ForEachElement(systemsElement, "System", [&](XMLElement *xmlSystem) {
            const char *systemType = xmlSystem->Attribute("type");
            if (!systemType)
                return;

            std::string systemName = systemType;

            Context context =
                Context{.application = &Application::GetInstance(), .scene = &scene, .world = scene.GetWorld()};

            if (systemName == "RenderSystem")
            {
                scene.AddSystem(std::make_unique<Elysium::Systems::RenderSystem>(context));
            }
            else if (systemName == "MovementSystem")
            {
                scene.AddSystem(std::make_unique<Elysium::Systems::MovementSystem>(context));
            }
            else if (systemName == "AnimationSystem")
            {
                scene.AddSystem(std::make_unique<Elysium::Systems::AnimationSystem>(context));
            }
            else if (systemName == "CameraSystem")
            {
                scene.AddSystem(std::make_unique<Elysium::Systems::CameraSystem>(context));
            }
            else if (systemName == "SpriteSystem")
            {
                scene.AddSystem(std::make_unique<Elysium::Systems::SpriteSystem>(context));
            }
            else if (systemName == "PickSystem")
            {
                scene.AddSystem(std::make_unique<Elysium::Systems::PickSystem>(context));
            }
            else
            {
                LOG_WARNINGF("Scene", "Unknown system type: %s", systemName.c_str());
            }
        });
    });
}

bool LoadScene(Scene &scene, const std::string &path)
{
    LOG_INFOF("Scene", "Loading scene from XML: %s", path.c_str());
    XMLDocument doc;

    if (!LoadXml(path, doc))
    {
        LOG_ERROR("Scene", "Failed to load scene file.");
        return false;
    }

    XMLElement *root = doc.FirstChildElement("Scene");
    if (!root)
    {
        LOG_ERROR("Scene", "Invalid scene file format. Missing root tag <Scene>.");
        return false;
    }

    World *world_ = scene.GetWorld();

    LoadLayers(root, world_);
    LoadTilemap(root, world_);
    LoadEntities(root, world_);
    LoadSystems(root, scene);

    // We allow the user to define the location and have the position component be implicit
    // Position component represents the CENTER of the tile in world coordinates
    world_->Query<LocationComponent>([&](Entity entity, auto &loc) {
        // Convert tile coordinates to centered world coordinates
        float worldX = loc.x * TILE_WIDTH + TILE_WIDTH * 0.5f;
        float worldY = loc.y * TILE_HEIGHT + TILE_HEIGHT * 0.5f;

        if (!world_->HasComponent<PositionComponent>(entity))
        {
            world_->AddComponent<PositionComponent>(entity, PositionComponent(worldX, worldY));
            return;
        }

        auto &pos = world_->GetComponent<PositionComponent>(entity);
        pos.x = worldX;
        pos.y = worldY;
    });

    // Allow subclasses to create additional custom systems
    scene.CreateCustomSystems();

    return true;
}
} // namespace Elysium

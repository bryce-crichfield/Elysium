#include "Scene.h"
#include "Services/LogService.h"
#include "Services/AssetService.h"
#include "Entity.h"
#include "System.h"
#include "Systems/RenderSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/AnimationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/SpriteSystem.h"
#include "tinyxml2.h"
#include "raylib.h"
#include <sstream>
#include <string>
#include "Xml.h"
#include "Application.h"

using namespace tinyxml2;

#define FOREACH(xmlName, varname, list) for (XMLElement* varname = list->FirstChildElement(xmlName); varname != nullptr; varname = varname->NextSiblingElement(xmlName))

namespace Elysium
{

template<typename Func>
void VisitElement(XMLElement* parent, const char* xmlName, Func func) {
    if (XMLElement* element = parent->FirstChildElement(xmlName)) {
        func(element);
    }
}

template<typename Func>
void VisitAttribute(XMLElement* element, const char* attrName, Func func) {
    if (const char* value = element->Attribute(attrName)) {
        func(value);
    }
}

// Iterates through child elements with a specific tag name
template<typename Func>
void ForEachElement(XMLElement* parent, const char* xmlName, Func func) {
    for (XMLElement* element = parent->FirstChildElement(xmlName);
         element != nullptr;
         element = element->NextSiblingElement(xmlName)) {
        func(element);
    }
}

// Iterates through all attributes of an element
template<typename Func>
void ForEachAttribute(XMLElement* element, Func func) {
    for (const XMLAttribute* attr = element->FirstAttribute();
         attr != nullptr;
         attr = attr->Next()) {
        func(attr);
    }
}

// Iterates through ALL child elements (regardless of tag name)
template<typename Func>
void ForEachChild(XMLElement* parent, Func func) {
    for (XMLElement* child = parent->FirstChildElement();
         child != nullptr;
         child = child->NextSiblingElement()) {
        func(child);
    }
}

void SceneSerializer::LoadScene(Scene &scene, const std::string &path)
{
    World* world_ = scene.GetWorld();

    LOG_INFOF("Scene", "Loading scene from XML: %s", path.c_str());
    XMLDocument doc;

    if (doc.LoadFile(path.c_str()) != XML_SUCCESS)
    {
        LOG_ERRORF("Scene", "Failed to load scene file: %s. Error: %s. Using defaults.", path.c_str(),
                   doc.ErrorStr());
        return;
    }

    LOG_INFO("Scene", "XML file loaded successfully");

    ProcessIncludes(doc, "");

    XMLElement *root = doc.FirstChildElement("Scene");
    if (!root)
    {
        LOG_ERROR("Scene", "Invalid scene file format. Using defaults.");
        return;
    }

    // Load layers first - these define the layer structure for rendering
    if (XMLElement *layersElement = root->FirstChildElement("Layers"))
    {
        LOG_DEBUG("Scene", "Processing Layers section");
        FOREACH("Layer", xmlLayer, layersElement)
        {
            const char *name = xmlLayer->Attribute("name");
            int zIndex = xmlLayer->IntAttribute("z", 0);
            const char *typeStr = xmlLayer->Attribute("type");
            const char *spaceStr = xmlLayer->Attribute("space");
            const char *blendStr = xmlLayer->Attribute("blend");
            float opacity = xmlLayer->FloatAttribute("opacity", 1.0f);
            bool isVisible = xmlLayer->BoolAttribute("isVisible", true);

            Entity layerEntity = world_->CreateEntity(std::string("Layer_") + (name ? name : "default"));

            LayerComponent layerComp;
            layerComp.zIndex = zIndex;
            layerComp.name = name ? name : "default";
            layerComp.opacity = opacity;
            layerComp.isVisible = isVisible;

            // Parse type - default to World for gameplay layers
            if (typeStr)
            {
                std::string type = typeStr;
                if (type == "Background")
                    layerComp.type = LayerComponent::Type::Background;
                else if (type == "World")
                    layerComp.type = LayerComponent::Type::World;
                else if (type == "Lighting")
                    layerComp.type = LayerComponent::Type::Lighting;
                else if (type == "Overlay")
                    layerComp.type = LayerComponent::Type::Overlay;
                else
                    layerComp.type = LayerComponent::Type::World;
            }
            else
            {
                layerComp.type = LayerComponent::Type::World;
            }

            // Parse space - default to World for gameplay layers
            if (spaceStr)
            {
                std::string space = spaceStr;
                if (space == "Screen")
                    layerComp.space = LayerComponent::Space::Screen;
                else if (space == "World")
                    layerComp.space = LayerComponent::Space::World;
                else if (space == "Parallax")
                    layerComp.space = LayerComponent::Space::Parallax;
                else
                    layerComp.space = LayerComponent::Space::World;
            }
            else
            {
                layerComp.space = LayerComponent::Space::World;
            }

            // Parse blend - default to Normal
            if (blendStr)
            {
                std::string blend = blendStr;
                if (blend == "Normal")
                    layerComp.blend = LayerComponent::Blend::Normal;
                else if (blend == "Additive")
                    layerComp.blend = LayerComponent::Blend::Additive;
                else if (blend == "Multiply")
                    layerComp.blend = LayerComponent::Blend::Multiply;
                else if (blend == "Alpha")
                    layerComp.blend = LayerComponent::Blend::Alpha;
                else
                    layerComp.blend = LayerComponent::Blend::Normal;
            }
            else
            {
                layerComp.blend = LayerComponent::Blend::Normal;
            }

            world_->AddComponent(layerEntity, layerComp);
            LOG_DEBUGF("Scene", "Created layer '%s' with z-index %d", layerComp.name.c_str(), layerComp.zIndex);
        }
    }

    // Load tilemaps
    if (XMLElement *tilemap = root->FirstChildElement("Tilemap"))
    {
        std::unordered_map<int, RectangleComponent> tileDefinitions;
        std::vector<int> tilemask;

        int tilemapWidth = tilemap->IntAttribute("width", 0);
        int tilemapHeight = tilemap->IntAttribute("height", 0);

        if (XMLElement *xmlTileMask = tilemap->FirstChildElement("Tilemask"))
        {
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
        }

        if (XMLElement *xmlTileDefinitions = tilemap->FirstChildElement("TileDefinitions"))
        {
            LOG_DEBUG("Scene", "Processing TileDefinitions");
            FOREACH("TileDefinition", xmlTileDefinition, xmlTileDefinitions)
            {
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
            }
        }

        for (int i = 0; i < tilemask.size(); i++)
        {
            int id = tilemask[i];
            int x = i % tilemapWidth;
            int y = i / tilemapWidth;
            auto entity = world_->CreateEntity();
            world_->AddComponent<LocationComponent>(entity, LocationComponent(x, y));
            world_->AddComponent<RectangleComponent>(entity, tileDefinitions[id]);
        }
    }

    // Load entities
    LOG_INFO("Scene", "Starting entity loading");
    int count = 0;
    if (XMLElement *entities = root->FirstChildElement("Entities"))
    {
        LOG_DEBUG("Scene", "Processing Entities section");
        FOREACH("Entity", xmlEntity, entities)
        {
            const char *entityName = xmlEntity->Attribute("name");
            Entity entity = world_->CreateEntity(entityName);
            count++;

            LOG_DEBUGF("Scene", "Created entity: %s", entityName ? entityName : "unnamed");

            for (XMLElement *component = xmlEntity->FirstChildElement(); component != nullptr;
                 component = component->NextSiblingElement())
            {
                // Parse the components
                std::string componentType = component->Name();
                LOG_DEBUGF("Scene", "Processing component: %s", componentType.c_str());

                if (componentType == "PositionComponent")
                {
                    float x = component->FloatAttribute("x", 0.0f);
                    float y = component->FloatAttribute("y", 0.0f);
                    world_->AddComponent(entity, PositionComponent(x, y));
                }
                else if (componentType == "LocationComponent")
                {
                    int x = component->IntAttribute("x", 0);
                    int y = component->IntAttribute("y", 0);
                    world_->AddComponent(entity, LocationComponent(x, y));
                }
                else if (componentType == "MovementComponent")
                {
                    std::vector<Vector2> waypoints;
                    if (XMLElement *xmlWaypoints = component->FirstChildElement("Waypoints"))
                    {
                        FOREACH("Waypoint", xmlWaypoint, xmlWaypoints)
                        {
                            int x = xmlWaypoint->IntAttribute("x", 0);
                            int y = xmlWaypoint->IntAttribute("y", 0);
                            waypoints.push_back({(float)x, (float)y});
                        }
                    }
                    world_->AddComponent(entity, MovementComponent(waypoints));
                }
                else if (componentType == "AnimationComponent")
                {
                    world_->AddComponent(entity, AnimationComponent());
                }
                else if (componentType == "DirectionComponent")
                {
                    world_->AddComponent(entity, DirectionComponent());
                }
                else if (componentType == "LayerComponent")
                {
                    int zIndex = component->IntAttribute("z", 0);
                    const char *name = component->Attribute("name");
                    const char *typeStr = component->Attribute("type");
                    const char *spaceStr = component->Attribute("space");
                    const char *blendStr = component->Attribute("blend");
                    float opacity = component->FloatAttribute("opacity", 1.0f);
                    bool isVisible = component->BoolAttribute("isVisible", true);

                    LayerComponent layerComp;
                    layerComp.zIndex = zIndex;
                    layerComp.name = name ? name : "default";
                    layerComp.opacity = opacity;
                    layerComp.isVisible = isVisible;

                    // Parse type
                    if (typeStr)
                    {
                        std::string type = typeStr;
                        if (type == "Background")
                            layerComp.type = LayerComponent::Type::Background;
                        else if (type == "World")
                            layerComp.type = LayerComponent::Type::World;
                        else if (type == "Lighting")
                            layerComp.type = LayerComponent::Type::Lighting;
                        else if (type == "Overlay")
                            layerComp.type = LayerComponent::Type::Overlay;
                        else
                            layerComp.type = LayerComponent::Type::World;
                    }
                    else
                    {
                        layerComp.type = LayerComponent::Type::World;
                    }

                    // Parse space
                    if (spaceStr)
                    {
                        std::string space = spaceStr;
                        if (space == "Screen")
                            layerComp.space = LayerComponent::Space::Screen;
                        else if (space == "World")
                            layerComp.space = LayerComponent::Space::World;
                        else if (space == "Parallax")
                            layerComp.space = LayerComponent::Space::Parallax;
                        else
                            layerComp.space = LayerComponent::Space::World;
                    }
                    else
                    {
                        layerComp.space = LayerComponent::Space::World;
                    }

                    // Parse blend
                    if (blendStr)
                    {
                        std::string blend = blendStr;
                        if (blend == "Normal")
                            layerComp.blend = LayerComponent::Blend::Normal;
                        else if (blend == "Additive")
                            layerComp.blend = LayerComponent::Blend::Additive;
                        else if (blend == "Multiply")
                            layerComp.blend = LayerComponent::Blend::Multiply;
                        else if (blend == "Alpha")
                            layerComp.blend = LayerComponent::Blend::Alpha;
                        else
                            layerComp.blend = LayerComponent::Blend::Normal;
                    }
                    else
                    {
                        layerComp.blend = LayerComponent::Blend::Normal;
                    }

                    world_->AddComponent(entity, layerComp);
                }
                else if (componentType == "RectangleComponent")
                {
                    float width = component->FloatAttribute("width", 100.0f);
                    float height = component->FloatAttribute("height", 100.0f);
                    std::string backgroundHex =
                        component->Attribute("background") ? component->Attribute("background") : "";
                    std::string borderHex = component->Attribute("border") ? component->Attribute("border") : "";
                    const char *layerName = component->Attribute("layerName");

                    Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                    Color borderColor = ParseHexColor(borderHex, BLANK);

                    world_->AddComponent(entity, RectangleComponent(width, height, backgroundColor, borderColor,
                                                                    layerName ? layerName : "default"));
                }
                else if (componentType == "CircleComponent")
                {
                    float radius = component->FloatAttribute("radius", 10.0f);
                    std::string backgroundHex =
                        component->Attribute("background") ? component->Attribute("background") : "";
                    std::string borderHex = component->Attribute("border") ? component->Attribute("border") : "";
                    const char *layerName = component->Attribute("layerName");

                    Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                    Color borderColor = ParseHexColor(borderHex, BLANK);

                    world_->AddComponent(entity, CircleComponent(radius, backgroundColor, borderColor,
                                                                 layerName ? layerName : "default"));
                }
                else if (componentType == "SpriteComponent")
                {
                    const char *spriteName = component->Attribute("spriteName");
                    const char *markerName = component->Attribute("markerName");
                    const char *layerName = component->Attribute("layerName");

                    if (spriteName && markerName)
                    {
                        auto &assetService =
                            Application::GetInstance().GetService<Elysium::Services::AssetService>("AssetService");
                        Sprite sprite = assetService.GetSprite(spriteName);

                        world_->AddComponent(entity,
                                             SpriteComponent(sprite, markerName, layerName ? layerName : "default"));
                    }
                    else
                    {
                        LOG_WARNING("Scene", "SpriteComponent missing required attributes: spriteName or markerName");
                    }
                }
                else if (componentType == "TextComponent")
                {
                    const char *content = component->Attribute("content");
                    int fontSize = component->IntAttribute("fontSize", 20);
                    int r = component->IntAttribute("r", 0);
                    int g = component->IntAttribute("g", 0);
                    int b = component->IntAttribute("b", 0);
                    int a = component->IntAttribute("a", 255);
                    const char *layerName = component->Attribute("layerName");
                    Color color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};

                    world_->AddComponent(entity, TextComponent(content ? content : "", fontSize, color,
                                                               layerName ? layerName : "default"));
                }

                else if (componentType == "CameraComponent")
                {
                    std::string target = component->Attribute("target") ? component->Attribute("target") : "";
                    world_->AddComponent(entity, CameraComponent());

                    // If there was a target, add a FollowComponent
                    if (!target.empty())
                    {
                        FollowComponent followComp;
                        followComp.targetEntityName = target;
                        world_->AddComponent(entity, followComp);
                    }
                }
                else if (componentType == "LightComponent")
                {
                    float radius = component->FloatAttribute("radius", 50.0f);
                    int r = component->IntAttribute("r", 255);
                    int g = component->IntAttribute("g", 255);
                    int b = component->IntAttribute("b", 255);
                    int a = component->IntAttribute("a", 100);
                    const char *layerName = component->Attribute("layerName");
                    Color color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
                    world_->AddComponent(entity, LightComponent(color, radius, layerName ? layerName : "default"));
                }
                else if (componentType == "TileComponent")
                {
                    world_->AddComponent(entity, TileComponent());
                }
                else if (componentType == "TeamComponent")
                {
                    int teamId = component->IntAttribute("teamId", 0);
                    world_->AddComponent(entity, TeamComponent(teamId));
                }

                else
                {
                    LOG_WARNINGF("Scene", "Unknown component type: %s", componentType.c_str());
                }
            }
        }
    }

    if (XMLElement *systemsElement = root->FirstChildElement("Systems"))
    {
        FOREACH("System", xmlSystem, systemsElement)
        {
            const char *systemType = xmlSystem->Attribute("type");
            if (!systemType)
                continue;

            std::string systemName = systemType;

            Context context = Context{.application = &Application::GetInstance(), .scene = &scene, .world = scene.GetWorld()};

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
            else
            {
                LOG_WARNINGF("Scene", "Unknown system type: %s", systemName.c_str());
            }
        }
    }

    // We allow the user to define the location and have the position component be implicit
    // Position component represents the CENTER of the tile in world coordinates
    world_->Query<LocationComponent>([&](Entity entity, auto &loc) {
        if (world_->HasComponent<TileComponent>(entity))
            return;

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
    // scene.CreateCustomSystems();
}

void SceneSerializer::SaveScene(const Scene &scene, const std::string &path)
{
    // Implementation for saving a scene to a file
}
} // namespace Elysium

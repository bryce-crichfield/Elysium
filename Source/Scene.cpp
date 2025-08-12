#include "Scene.h"
#include "Entity.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/AnimationSystem.h"
#include "Systems/CameraSystem.h"
#include "tinyxml2.h"
#include "raylib.h"
#include <sstream>
#include <string>
#include "Application.h"

using namespace tinyxml2;

#define FOREACH(xmlName, varname, list) for (XMLElement* varname = list->FirstChildElement(xmlName); varname != nullptr; varname = varname->NextSiblingElement(xmlName))

namespace Elysium {

Scene::Scene(const std::string& name) : name_(name) {
    world_ = std::make_unique<World>();
}

Scene::~Scene() {

}

Color ParseHexColor(const std::string& hex, Color defaultColor = BLANK) {
    if (hex.empty() || hex[0] != '#') return defaultColor;

    std::string colorStr = hex.substr(1); // Remove '#'

    if (colorStr.length() == 6) {
        // #RRGGBB format
        unsigned int colorValue = std::stoul(colorStr, nullptr, 16);
        return {
            (unsigned char)((colorValue >> 16) & 0xFF), // R
            (unsigned char)((colorValue >> 8) & 0xFF),  // G
            (unsigned char)(colorValue & 0xFF),         // B
            255                                         // A (full opacity)
        };
    } else if (colorStr.length() == 8) {
        // #RRGGBBAA format
        unsigned int colorValue = std::stoul(colorStr, nullptr, 16);
        return {
            (unsigned char)((colorValue >> 24) & 0xFF), // R
            (unsigned char)((colorValue >> 16) & 0xFF), // G
            (unsigned char)((colorValue >> 8) & 0xFF),  // B
            (unsigned char)(colorValue & 0xFF)          // A
        };
    }

    return defaultColor;
}

void ProcessIncludes(XMLDocument& doc, const std::string& basePath) {
    // Find all <include> elements
    for (XMLElement* includeElem = doc.RootElement()->FirstChildElement("Include");
         includeElem != nullptr;
         includeElem = includeElem->NextSiblingElement("Include"))
    {
        const char* src = includeElem->Attribute("src");
        if (!src) continue;

        // Load the included file
        XMLDocument includeDoc;
        if (includeDoc.LoadFile((basePath + src).c_str()) == XML_SUCCESS) {
            // Grab the root node of the included file
            XMLElement* includedRoot = includeDoc.RootElement();
            if (includedRoot) {
                // Deep clone into main document
                XMLElement* clone = includedRoot->DeepClone(&doc)->ToElement();
                includeElem->Parent()->InsertAfterChild(includeElem, clone);
            }
        } else {
            TraceLog(LOG_ERROR, "Failed to load include");
            // std::cerr << "Failed to load include: " << src << "\n";
        }

        // Remove the <include> tag
        includeElem->Parent()->DeleteChild(includeElem);
        // Restart search from the beginning after modification
        includeElem = doc.RootElement()->FirstChildElement("Include");
    }
}

void Scene::LoadFromXML(const std::string& xmlPath) {
    TraceLog(LOG_INFO, "Loading scene from XML: %s", xmlPath.c_str());
    XMLDocument doc;

    if (doc.LoadFile(xmlPath.c_str()) != XML_SUCCESS) {
        TraceLog(LOG_ERROR, "Failed to load scene file: %s. Error: %s. Using defaults.",
                xmlPath.c_str(), doc.ErrorStr());
        return;
    }
    
    TraceLog(LOG_INFO, "XML file loaded successfully");

    ProcessIncludes(doc, "");

    XMLElement *root = doc.FirstChildElement("Scene");
    if (!root) {
        TraceLog(LOG_ERROR, "Invalid scene file format. Using defaults.");
        return;
    }

    // Load layers first - these define the layer structure for rendering
    if (XMLElement *layersElement = root->FirstChildElement("Layers")) {
        TraceLog(LOG_INFO, "Processing Layers section");
        FOREACH("Layer", xmlLayer, layersElement) {
            const char* name = xmlLayer->Attribute("name");
            int zIndex = xmlLayer->IntAttribute("z", 0);
            const char* typeStr = xmlLayer->Attribute("type");
            const char* spaceStr = xmlLayer->Attribute("space");
            const char* blendStr = xmlLayer->Attribute("blend");
            float opacity = xmlLayer->FloatAttribute("opacity", 1.0f);
            bool isVisible = xmlLayer->BoolAttribute("isVisible", true);
            
            Entity layerEntity = world_->CreateEntity(std::string("Layer_") + (name ? name : "default"));
            
            LayerComponent layerComp;
            layerComp.zIndex = zIndex;
            layerComp.name = name ? name : "default";
            layerComp.opacity = opacity;
            layerComp.isVisible = isVisible;
            
            // Parse type - default to World for gameplay layers
            if (typeStr) {
                std::string type = typeStr;
                if (type == "Background") layerComp.type = LayerComponent::Type::Background;
                else if (type == "World") layerComp.type = LayerComponent::Type::World;
                else if (type == "Lighting") layerComp.type = LayerComponent::Type::Lighting;
                else if (type == "Overlay") layerComp.type = LayerComponent::Type::Overlay;
                else layerComp.type = LayerComponent::Type::World;
            } else {
                layerComp.type = LayerComponent::Type::World;
            }
            
            // Parse space - default to World for gameplay layers
            if (spaceStr) {
                std::string space = spaceStr;
                if (space == "Screen") layerComp.space = LayerComponent::Space::Screen;
                else if (space == "World") layerComp.space = LayerComponent::Space::World;
                else if (space == "Parallax") layerComp.space = LayerComponent::Space::Parallax;
                else layerComp.space = LayerComponent::Space::World;
            } else {
                layerComp.space = LayerComponent::Space::World;
            }
            
            // Parse blend - default to Normal
            if (blendStr) {
                std::string blend = blendStr;
                if (blend == "Normal") layerComp.blend = LayerComponent::Blend::Normal;
                else if (blend == "Additive") layerComp.blend = LayerComponent::Blend::Additive;
                else if (blend == "Multiply") layerComp.blend = LayerComponent::Blend::Multiply;
                else if (blend == "Alpha") layerComp.blend = LayerComponent::Blend::Alpha;
                else layerComp.blend = LayerComponent::Blend::Normal;
            } else {
                layerComp.blend = LayerComponent::Blend::Normal;
            }
            
            world_->AddComponent(layerEntity, layerComp);
            TraceLog(LOG_INFO, "Created layer '%s' with z-index %d", layerComp.name.c_str(), layerComp.zIndex);
        }
    }

    // Load tilemaps
    if (XMLElement *tilemap = root->FirstChildElement("Tilemap")) {
        std::unordered_map<int, RectangleComponent> tileDefinitions;
        std::vector<int> tilemask;

        int tilemapWidth = tilemap->IntAttribute("width", 0);
        int tilemapHeight = tilemap->IntAttribute("height", 0);

        if (XMLElement *xmlTileMask = tilemap->FirstChildElement("Tilemask")) {
            const char* textContent = xmlTileMask->GetText();
            if (textContent) {
                std::string content = std::string(textContent);

                // Remove leading and trailing whitespace
                size_t start = content.find_first_not_of(" \t\n\r");
                size_t end = content.find_last_not_of(" \t\n\r");

                if (start != std::string::npos && end != std::string::npos) {
                    content = content.substr(start, end - start + 1);

                    // Split by whitespace and convert to integers
                    std::istringstream iss(content);
                    std::string token;
                    while (iss >> token) {
                        try {
                            int value = std::stoi(token);
                            tilemask.push_back(value);
                        } catch (const std::exception& e) {
                            throw std::exception(e);
                        }
                    }
                }
            }
        }

        if (XMLElement *xmlTileDefinitions = tilemap->FirstChildElement("TileDefinitions")) {
            TraceLog(LOG_INFO, "Processing TileDefinitions");
            FOREACH("TileDefinition", xmlTileDefinition, xmlTileDefinitions) {
                int id = xmlTileDefinition->IntAttribute("id", 0);
                std::string backgroundHex = xmlTileDefinition->Attribute("background") ? xmlTileDefinition->Attribute("background") : "";
                std::string borderHex = xmlTileDefinition->Attribute("border") ? xmlTileDefinition->Attribute("border") : "";
                const char* layerName = xmlTileDefinition->Attribute("layerName");
                Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                Color borderColor = ParseHexColor(borderHex, BLANK);

                tileDefinitions[id] = RectangleComponent(32, 32, backgroundColor, borderColor, layerName ? layerName : "tile");
                TraceLog(LOG_INFO, "Created tile definition %d with layer '%s'", id, layerName ? layerName : "tile");
            }
        }

        for (int i = 0; i < tilemask.size(); i++) {
            int id = tilemask[i];
            int x = i % tilemapWidth;
            int y = i / tilemapWidth;
            auto entity = world_->CreateEntity();
            world_->AddComponent<LocationComponent>(entity, LocationComponent(x, y));
            world_->AddComponent<RectangleComponent>(entity, tileDefinitions[id]);
        }
    }

    // Load entities
    TraceLog(LOG_INFO, "Starting entity loading");
    int count = 0;
    if (XMLElement *entities = root->FirstChildElement("Entities")) {
        TraceLog(LOG_INFO, "Processing Entities section");
        FOREACH("Entity", xmlEntity, entities) {
            const char* entityName = xmlEntity->Attribute("name");
            Entity entity = world_->CreateEntity(entityName);
            count++;

            TraceLog(LOG_INFO, "Created entity: %s", entityName ? entityName : "unnamed");
            
            for (XMLElement* component = xmlEntity->FirstChildElement();
                component != nullptr;
                component = component->NextSiblingElement()) {
                // Parse the components
                std::string componentType = component->Name();
                TraceLog(LOG_INFO, "Processing component: %s", componentType.c_str());

                if (componentType == "PositionComponent") {
                    float x = component->FloatAttribute("x", 0.0f);
                    float y = component->FloatAttribute("y", 0.0f);
                    world_->AddComponent(entity, PositionComponent(x, y));
                }
                else if (componentType == "LocationComponent") {
                    int x = component->IntAttribute("x", 0);
                    int y = component->IntAttribute("y", 0);
                    world_->AddComponent(entity, LocationComponent(x, y));
                }
                else if (componentType == "VelocityComponent") {
                    float x = component->FloatAttribute("x", 0.0f);
                    float y = component->FloatAttribute("y", 0.0f);
                    world_->AddComponent(entity, VelocityComponent(x, y));
                }
                else if (componentType == "AnimationComponent") {
                    world_->AddComponent(entity, AnimationComponent());
                }
                else if (componentType == "LayerComponent") {
                    int zIndex = component->IntAttribute("z", 0);
                    const char* name = component->Attribute("name");
                    const char* typeStr = component->Attribute("type");
                    const char* spaceStr = component->Attribute("space");
                    const char* blendStr = component->Attribute("blend");
                    float opacity = component->FloatAttribute("opacity", 1.0f);
                    bool isVisible = component->BoolAttribute("isVisible", true);
                    
                    LayerComponent layerComp;
                    layerComp.zIndex = zIndex;
                    layerComp.name = name ? name : "default";
                    layerComp.opacity = opacity;
                    layerComp.isVisible = isVisible;
                    
                    // Parse type
                    if (typeStr) {
                        std::string type = typeStr;
                        if (type == "Background") layerComp.type = LayerComponent::Type::Background;
                        else if (type == "World") layerComp.type = LayerComponent::Type::World;
                        else if (type == "Lighting") layerComp.type = LayerComponent::Type::Lighting;
                        else if (type == "Overlay") layerComp.type = LayerComponent::Type::Overlay;
                        else layerComp.type = LayerComponent::Type::World;
                    } else {
                        layerComp.type = LayerComponent::Type::World;
                    }
                    
                    // Parse space
                    if (spaceStr) {
                        std::string space = spaceStr;
                        if (space == "Screen") layerComp.space = LayerComponent::Space::Screen;
                        else if (space == "World") layerComp.space = LayerComponent::Space::World;
                        else if (space == "Parallax") layerComp.space = LayerComponent::Space::Parallax;
                        else layerComp.space = LayerComponent::Space::World;
                    } else {
                        layerComp.space = LayerComponent::Space::World;
                    }
                    
                    // Parse blend
                    if (blendStr) {
                        std::string blend = blendStr;
                        if (blend == "Normal") layerComp.blend = LayerComponent::Blend::Normal;
                        else if (blend == "Additive") layerComp.blend = LayerComponent::Blend::Additive;
                        else if (blend == "Multiply") layerComp.blend = LayerComponent::Blend::Multiply;
                        else if (blend == "Alpha") layerComp.blend = LayerComponent::Blend::Alpha;
                        else layerComp.blend = LayerComponent::Blend::Normal;
                    } else {
                        layerComp.blend = LayerComponent::Blend::Normal;
                    }
                    
                    world_->AddComponent(entity, layerComp);
                }
                else if (componentType == "RectangleComponent") {
                    float width = component->FloatAttribute("width", 100.0f);
                    float height = component->FloatAttribute("height", 100.0f);
                    std::string backgroundHex = component->Attribute("background") ? component->Attribute("background") : "";
                    std::string borderHex = component->Attribute("border") ? component->Attribute("border") : "";
                    const char* layerName = component->Attribute("layerName");

                    Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                    Color borderColor = ParseHexColor(borderHex, BLANK);

                    world_->AddComponent(entity, RectangleComponent(width, height, backgroundColor, borderColor, layerName ? layerName : "default"));
                }
                else if (componentType == "CircleComponent") {
                    float radius = component->FloatAttribute("radius", 10.0f);
                    std::string backgroundHex = component->Attribute("background") ? component->Attribute("background") : "";
                    std::string borderHex = component->Attribute("border") ? component->Attribute("border") : "";
                    const char* layerName = component->Attribute("layerName");

                    Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                    Color borderColor = ParseHexColor(borderHex, BLANK);

                    world_->AddComponent(entity, CircleComponent(radius, backgroundColor, borderColor, layerName ? layerName : "default"));
                }
                else if (componentType == "PhysicsComponent") {
                    float mass = component->FloatAttribute("mass", 1.0f);
                    float restitution = component->FloatAttribute("restitution", 0.8f);
                    float friction = component->FloatAttribute("friction", 0.1f);
                    bool affectedByGravity = component->BoolAttribute("affectedByGravity", true);
                    world_->AddComponent(entity, PhysicsComponent(mass, restitution, friction, affectedByGravity));
                }
                else if (componentType == "SpriteComponent") {
                    const char* spriteName = component->Attribute("spriteName");
                    const char* markerName = component->Attribute("markerName");
                    const char* layerName = component->Attribute("layerName");

                    if (spriteName && markerName) {
                        auto& assetService = Application::GetInstance().GetAssetService();
                        Sprite sprite = assetService.GetSprite(spriteName);
                        
                        world_->AddComponent(entity, SpriteComponent(
                            sprite,
                            markerName,
                            layerName ? layerName : "default"
                        ));
                    } else {
                        TraceLog(LOG_WARNING, "SpriteComponent missing required attributes: spriteName or markerName");
                    }
                }
                else if (componentType == "TextComponent") {
                    const char* content = component->Attribute("content");
                    int fontSize = component->IntAttribute("fontSize", 20);
                    int r = component->IntAttribute("r", 0);
                    int g = component->IntAttribute("g", 0);
                    int b = component->IntAttribute("b", 0);
                    int a = component->IntAttribute("a", 255);
                    const char* layerName = component->Attribute("layerName");
                    Color color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};

                    world_->AddComponent(entity, TextComponent(
                        content ? content : "",
                        fontSize,
                        color,
                        layerName ? layerName : "default"
                    ));
                }

                else if (componentType == "CameraComponent") {
                    std::string target = component->Attribute("target") ? component->Attribute("target") : "";
                    world_->AddComponent(entity, CameraComponent());
                    
                    // If there was a target, add a FollowComponent
                    if (!target.empty()) {
                        FollowComponent followComp;
                        followComp.targetEntityName = target;
                        world_->AddComponent(entity, followComp);
                    }
                }
                else if (componentType == "LightComponent") {
                    float radius = component->FloatAttribute("radius", 50.0f);
                    int r = component->IntAttribute("r", 255);
                    int g = component->IntAttribute("g", 255);
                    int b = component->IntAttribute("b", 255);
                    int a = component->IntAttribute("a", 100);
                    const char* layerName = component->Attribute("layerName");
                    Color color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
                    world_->AddComponent(entity, LightComponent(color, radius, layerName ? layerName : "default"));
                }
                else if (componentType == "TileComponent") {
                    world_->AddComponent(entity, TileComponent());
                }
                else if (componentType == "TeamComponent") {
                    int teamId = component->IntAttribute("teamId", 0);
                    world_->AddComponent(entity, TeamComponent(teamId));
                }

                else {
                    TraceLog(LOG_WARNING, "Unknown component type: %s", componentType.c_str());
                }
            }
        }
    }

    // Load systems
    if (XMLElement *systemsElement = root->FirstChildElement("Systems")) {
        FOREACH("System", xmlSystem, systemsElement) {
            const char* systemType = xmlSystem->Attribute("type");
            if (!systemType) continue;

            std::string systemName = systemType;

            Context context = Context {
                .application = &Application::GetInstance(),
                .scene = this,
                .world = world_.get()
            };

            if (systemName == "PhysicsSystem") {
                float gravity = xmlSystem->FloatAttribute("gravity", 500.0f);
                systems_.push_back(std::make_unique<Elysium::Systems::PhysicsSystem>(context, gravity));
                TraceLog(LOG_INFO, "Loaded PhysicsSystem with gravity: %.2f", gravity);
            }
            else if (systemName == "RenderSystem") {
                systems_.push_back(std::make_unique<Elysium::Systems::RenderSystem>(context));
                TraceLog(LOG_INFO, "Loaded RenderSystem");
            }
            else if (systemName == "AnimationSystem") {
                systems_.push_back(std::make_unique<Elysium::Systems::AnimationSystem>(context));
                TraceLog(LOG_INFO, "Loaded AnimationSystem");
            }
            else if (systemName == "CameraSystem") {
                systems_.push_back(std::make_unique<Elysium::Systems::CameraSystem>(context));
                TraceLog(LOG_INFO, "Loaded CameraSystem");
            }
            else {
                TraceLog(LOG_WARNING, "Unknown system type: %s", systemName.c_str());
            }
        }
    }

    // We allow the user to define the location and have the position component be implicit
    // Position component represents the CENTER of the tile in world coordinates
    world_->ForEachEntityWith<LocationComponent>([&](Entity entity) {
        if (world_->HasComponent<TileComponent>(entity)) return;
        auto& loc = world_->GetComponent<LocationComponent>(entity);

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
    CreateCustomSystems();
}

void Scene::OnUpdate(float deltaTime) {
    for (auto& system : systems_) {
        system->Update(deltaTime);
    }
}

void Scene::OnDraw(Rectangle screen) {
    // RenderSystem now handles all camera rendering internally
    // Just render all systems - no need for manual camera management
    for (auto& system : systems_) {
        system->Render();
    }
}

void Scene::OnDebugDraw() {
}

} // namespace Elysium

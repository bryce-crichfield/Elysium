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
    XMLDocument doc;

    if (doc.LoadFile(xmlPath.c_str()) != XML_SUCCESS) {
        TraceLog(LOG_ERROR, "Failed to load scene file: %s. Error: %s. Using defaults.",
                xmlPath.c_str(), doc.ErrorStr());
        return;
    }

    ProcessIncludes(doc, "");

    XMLElement *root = doc.FirstChildElement("Scene");
    if (!root) {
        TraceLog(LOG_ERROR, "Invalid scene file format. Using defaults.");
        return;
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
            FOREACH("TileDefinition", xmlTileDefinition, xmlTileDefinitions) {
                int id = xmlTileDefinition->IntAttribute("id", 0);
                std::string backgroundHex = xmlTileDefinition->Attribute("background") ? xmlTileDefinition->Attribute("background") : "";
                std::string borderHex = xmlTileDefinition->Attribute("border") ? xmlTileDefinition->Attribute("border") : "";
                Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                Color borderColor = ParseHexColor(borderHex, BLANK);

                tileDefinitions[id] = RectangleComponent(32, 32, backgroundColor, borderColor);
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
    int count = 0;
    if (XMLElement *entities = root->FirstChildElement("Entities")) {
        FOREACH("Entity", xmlEntity, entities) {
            const char* entityName = xmlEntity->Attribute("name");
            Entity entity = world_->CreateEntity(entityName);
            count++;

            for (XMLElement* component = xmlEntity->FirstChildElement();
                component != nullptr;
                component = component->NextSiblingElement()) {
                // Parse the components
                std::string componentType = component->Name();

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
                    int z = component->IntAttribute("z");
                    world_->AddComponent(entity, LayerComponent(z));
                }
                else if (componentType == "RectangleComponent") {
                    float width = component->FloatAttribute("width", 100.0f);
                    float height = component->FloatAttribute("height", 100.0f);
                    std::string backgroundHex = component->Attribute("background") ? component->Attribute("background") : "";
                    std::string borderHex = component->Attribute("border") ? component->Attribute("border") : "";

                    Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                    Color borderColor = ParseHexColor(borderHex, BLANK);

                    world_->AddComponent(entity, RectangleComponent(width, height, backgroundColor, borderColor));
                }
                else if (componentType == "CircleComponent") {
                    float radius = component->FloatAttribute("radius", 10.0f);
                    std::string backgroundHex = component->Attribute("background") ? component->Attribute("background") : "";
                    std::string borderHex = component->Attribute("border") ? component->Attribute("border") : "";

                    Color backgroundColor = ParseHexColor(backgroundHex, BLANK);
                    Color borderColor = ParseHexColor(borderHex, BLANK);

                    world_->AddComponent(entity, CircleComponent(radius, backgroundColor, borderColor));
                }
                else if (componentType == "PhysicsComponent") {
                    float mass = component->FloatAttribute("mass", 1.0f);
                    float restitution = component->FloatAttribute("restitution", 0.8f);
                    float friction = component->FloatAttribute("friction", 0.1f);
                    bool affectedByGravity = component->BoolAttribute("affectedByGravity", true);
                    world_->AddComponent(entity, PhysicsComponent(mass, restitution, friction, affectedByGravity));
                }
                else if (componentType == "SpriteComponent") {
                    const char* textureName = component->Attribute("textureName");
                    const char* frame = component->Attribute("frame");
                    float scaleX = component->FloatAttribute("scaleX", 1.0f);
                    float scaleY = component->FloatAttribute("scaleY", 1.0f);
                    float rotation = component->FloatAttribute("rotation", 0.0f);
                    int r = component->IntAttribute("r", 255);
                    int g = component->IntAttribute("g", 255);
                    int b = component->IntAttribute("b", 255);
                    int a = component->IntAttribute("a", 255);
                    Color tint = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};

                    world_->AddComponent(entity, SpriteComponent(
                        textureName ? textureName : "",
                        frame ? frame : "",
                        {scaleX, scaleY},
                        rotation,
                        tint
                    ));
                }
                else if (componentType == "TextComponent") {
                    const char* content = component->Attribute("content");
                    int fontSize = component->IntAttribute("fontSize", 20);
                    int r = component->IntAttribute("r", 0);
                    int g = component->IntAttribute("g", 0);
                    int b = component->IntAttribute("b", 0);
                    int a = component->IntAttribute("a", 255);
                    Color color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};

                    world_->AddComponent(entity, TextComponent(
                        content ? content : "",
                        fontSize,
                        color
                    ));
                }

                else if (componentType == "CameraComponent") {
                    std::string target = component->Attribute("target") ? component->Attribute("target") : "";
                    world_->AddComponent(entity, CameraComponent(target));
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
    // Find camera system and begin camera mode
    Elysium::Systems::CameraSystem* cameraSystem = nullptr;
    for (auto& system : systems_) {
        cameraSystem = dynamic_cast<Elysium::Systems::CameraSystem*>(system.get());
        if (cameraSystem) break;
    }

    // Begin camera mode if we have a camera system with active camera
    if (cameraSystem && cameraSystem->HasActiveCamera()) {
        cameraSystem->BeginCameraMode();
    }

    // Render all systems
    for (auto& system : systems_) {
        system->Render();
    }

    // End camera mode if it was started
    if (cameraSystem && cameraSystem->HasActiveCamera()) {
        cameraSystem->EndCameraMode();
    }
}

void Scene::OnDebugDraw() {
}

} // namespace Elysium

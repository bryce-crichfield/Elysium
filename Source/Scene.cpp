#include "Scene.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "tinyxml2.h"
#include "raylib.h"

using namespace tinyxml2;

#define FOREACH(xmlName, varname, list) for (XMLElement* varname = list->FirstChildElement(xmlName); varname != nullptr; varname = varname->NextSiblingElement(xmlName))

namespace Elysium {

Scene::Scene(const std::string& name) : name_(name) {
    world_ = std::make_unique<EntityWorld>();
}

void Scene::LoadFromXML(const std::string& xmlPath) {
    XMLDocument doc;

    if (doc.LoadFile(xmlPath.c_str()) != XML_SUCCESS) {
        TraceLog(LOG_ERROR, "Failed to load scene file: %s. Error: %s. Using defaults.", 
                xmlPath.c_str(), doc.ErrorStr());
        return;
    }

    XMLElement *root = doc.FirstChildElement("Scene");
    if (!root) {
        TraceLog(LOG_ERROR, "Invalid scene file format. Using defaults.");
        return;
    }

    // Load entities
    if (XMLElement *entities = root->FirstChildElement("Entities")) {
        FOREACH("Entity", xmlEntity, entities) {
            const char* entityName = xmlEntity->Attribute("name");
            Entity entity = world_->CreateEntity();

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
                else if (componentType == "VelocityComponent") {
                    float x = component->FloatAttribute("x", 0.0f);
                    float y = component->FloatAttribute("y", 0.0f);
                    world_->AddComponent(entity, VelocityComponent(x, y));
                }
                else if (componentType == "CircleRenderComponent") {
                    float radius = component->FloatAttribute("radius", 10.0f);
                    int r = component->IntAttribute("r", 255);
                    int g = component->IntAttribute("g", 255);
                    int b = component->IntAttribute("b", 255);
                    int a = component->IntAttribute("a", 255);
                    Color color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
                    
                    bool drawOutline = component->BoolAttribute("drawOutline", true);
                    int or_ = component->IntAttribute("outlineR", 0);
                    int og = component->IntAttribute("outlineG", 0);
                    int ob = component->IntAttribute("outlineB", 0);
                    int oa = component->IntAttribute("outlineA", 255);
                    Color outlineColor = {(unsigned char)or_, (unsigned char)og, (unsigned char)ob, (unsigned char)oa};
                    
                    world_->AddComponent(entity, CircleRenderComponent(radius, color, drawOutline, outlineColor));
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
            
            if (systemName == "PhysicsSystem") {
                float gravity = xmlSystem->FloatAttribute("gravity", 500.0f);
                systems_.push_back(std::make_unique<Elysium::Systems::PhysicsSystem>(world_.get(), gravity));
                TraceLog(LOG_INFO, "Loaded PhysicsSystem with gravity: %.2f", gravity);
            }
            else if (systemName == "RenderSystem") {
                systems_.push_back(std::make_unique<Elysium::Systems::RenderSystem>(world_.get()));
                TraceLog(LOG_INFO, "Loaded RenderSystem");
            }
            else {
                TraceLog(LOG_WARNING, "Unknown system type: %s", systemName.c_str());
            }
        }
    }
    
    // Allow subclasses to create additional custom systems
    CreateCustomSystems();
}

void Scene::OnUpdate(float deltaTime) {
    for (auto& system : systems_) {
        system->Update(deltaTime);
    }
}

void Scene::OnDraw(Rectangle screen) {
    for (auto& system : systems_) {
        system->Render();
    }
}

} // namespace Elysium
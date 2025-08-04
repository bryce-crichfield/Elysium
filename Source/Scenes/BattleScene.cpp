#include "Scenes/MenuScene.h"
#include "Application.h"
#include "Scenes/GameScene.h"
#include "Scenes/BattleScene.h"
#include "imgui.h"
#include <memory>
#include "tinyxml2.h"

using namespace tinyxml2;

#define FOREACH(xmlName, varname, list) for (XMLElement* varname = list->FirstChildElement(xmlName); varname != nullptr; varname = varname->NextSiblingElement(xmlName))

namespace Elysium::Scenes {


void LoadBattleScene(std::string xmlPath, EntityWorld* world, std::vector<std::unique_ptr<System>>* systems)
{
    XMLDocument doc;

    if (doc.LoadFile(xmlPath.c_str()) != XML_SUCCESS)
    {
        TraceLog(LOG_ERROR, "Failed to load config file: %s. Error: %s. Using defaults.", 
                xmlPath.c_str(), doc.ErrorStr());
        return;
    }

    XMLElement *root = doc.FirstChildElement("Scene");
    if (!root)
    {
        TraceLog(LOG_ERROR, "Invalid config file format. Using defaults.");
        return;
    }

    if (XMLElement *camera = root->FirstChildElement("Camera"))
    {
    }

    if (XMLElement *layers = root->FirstChildElement("Layers"))
    {
        FOREACH("Layer", layer, layers)
        {
            const char* layerName = layer->Attribute("name");
            const char* layerIndex = layer->Attribute("index");
            
            printf("Layer: %s, Index: %s\n", layerName ? layerName : "unnamed", 
                layerIndex ? layerIndex : "unknown");
        }
    }

    if (XMLElement *entities = root->FirstChildElement("Entities"))
    {
        FOREACH("Entity", xmlEntity, entities)
        {
            const char* entityName = xmlEntity->Attribute("name");
            Entity entity = world->CreateEntity();

            for (XMLElement* component = xmlEntity->FirstChildElement(); 
                component != nullptr; 
                component = component->NextSiblingElement())
            {
                // Parse the components
                std::string componentType = component->Name();

                if (componentType == "PositionComponent") {
                    float x = component->FloatAttribute("x", 0.0f);
                    float y = component->FloatAttribute("y", 0.0f);
                    world->AddComponent(entity, PositionComponent(x, y));
                }
                else if (componentType == "VelocityComponent") {
                    float x = component->FloatAttribute("x", 0.0f);
                    float y = component->FloatAttribute("y", 0.0f);
                    world->AddComponent(entity, VelocityComponent(x, y));
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
                    
                    world->AddComponent(entity, CircleRenderComponent(radius, color, drawOutline, outlineColor));
                }
                else if (componentType == "PhysicsComponent") {
                    float mass = component->FloatAttribute("mass", 1.0f);
                    float restitution = component->FloatAttribute("restitution", 0.8f);
                    float friction = component->FloatAttribute("friction", 0.1f);
                    bool affectedByGravity = component->BoolAttribute("affectedByGravity", true);
                    world->AddComponent(entity, PhysicsComponent(mass, restitution, friction, affectedByGravity));
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
                    
                    world->AddComponent(entity, SpriteComponent(
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
                    
                    world->AddComponent(entity, TextComponent(
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

    if (XMLElement *systemsElement = root->FirstChildElement("Systems"))
    {
        FOREACH("System", xmlSystem, systemsElement)
        {
            const char* systemType = xmlSystem->Attribute("type");
            if (!systemType) continue;

            std::string systemName = systemType;
            
            if (systemName == "PhysicsSystem") {
                float gravity = xmlSystem->FloatAttribute("gravity", 500.0f);
                systems->push_back(std::make_unique<PhysicsSystem>(world, gravity));
                TraceLog(LOG_INFO, "Loaded PhysicsSystem with gravity: %.2f", gravity);
            }
            else if (systemName == "RenderSystem") {
                systems->push_back(std::make_unique<RenderSystem>(world));
                TraceLog(LOG_INFO, "Loaded RenderSystem");
            }
            else {
                TraceLog(LOG_WARNING, "Unknown system type: %s", systemName.c_str());
            }
        }
    }
}

BattleScene::BattleScene(const GameConfig& config) : Scene("BattleScene", config) {
    world = std::make_unique<EntityWorld>();
}

void BattleScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Battle Scene");

    LoadBattleScene("./Assets/Scene.xml", world.get(), &systems);
}

void BattleScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Menu Scene");
}

void BattleScene::OnInput(const InputEvent& event) {
  
}

void BattleScene::OnUpdate(float deltaTime) {
    for (auto& system : systems) {
        system->Update(deltaTime);
    }
}

void BattleScene::OnDraw() {
    for (auto& system : systems) {
        system->Render();
    }
}

void BattleScene::OnDebugDraw() {
    // ImGui Scene Manager Window
    ImGui::Begin("Scene Manager");
    
    ImGui::Text("Current Scene: %s", GetName().c_str());
    ImGui::Separator();
    
    ImGui::Text("Available Scenes:");
    
    if (ImGui::Button("Switch to Game Scene", ImVec2(200, 30))) {
        auto gameScene = std::make_unique<GameScene>(config_);
        Elysium::Application::GetInstance().QueueSceneTransition(std::move(gameScene));
    }

    if (ImGui::Button("Switch to Battle Scene", ImVec2(200, 30))) {
        auto battleScene = std::make_unique<BattleScene>(config_);
        Elysium::Application::GetInstance().QueueSceneTransition(std::move(battleScene));
    }
    
    ImGui::Separator();
    
    ImGui::End();
}


} // namespace Elysium::Scenes
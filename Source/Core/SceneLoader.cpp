#include <filesystem>
#include <sstream>
#include <string>
#include "Core/Log.h"
#include "Core/ServiceLocator.h"
#include "Entity.h"
#include "Core/Script.h"
#include "Interfaces/IAssetService.h"
#include "Scene.h"
#include "System.h"
#include "Core/Xml.h"
#include "Core/ComponentRegistry.h"
#include "Core/SystemRegistry.h"
#include "Core/Components.h"
#include "Core/Path.h"
#include "Core/PrefabInstance.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium {

namespace {

using ComponentLoader = std::function<void(XMLElement*, World*, Entity, ServiceLocator&)>;

// XML tag -> component loader, from the registry, with the CameraComponent special case
// layered on top: a "target" also stamps FollowComponent + ParentComponent.
const std::unordered_map<std::string, ComponentLoader>& ComponentLoaders() {
    static const std::unordered_map<std::string, ComponentLoader> loaders = [] {
        std::unordered_map<std::string, ComponentLoader> result;
        for (const auto& [name, loader] : ComponentRegistry::Instance().GetXmlLoaders()) result[name] = loader;

        result["CameraComponent"] = [](XMLElement* xmlComponent, World* world, Entity entity, ServiceLocator& services) {
            CameraComponent cam{};
            CameraComponent::LoadXml(cam, xmlComponent, services);
            world->AddComponent(entity, cam);

            std::string target = xmlComponent->Attribute("target") ? xmlComponent->Attribute("target") : "";
            if (!target.empty()) {
                world->AddComponent(entity, FollowComponent{});  // speed=0 -> instant by default
                ParentComponent parentComp;
                parentComp.targetName = target;
                world->AddComponent(entity, parentComp);
            }
        };
        return result;
    }();
    return loaders;
}

}  // namespace

void LoadEntityComponents(XMLElement* xmlEntity, World* world, Entity entity, ServiceLocator& services) {
    ForEachChild(xmlEntity, [&](XMLElement* component) {
        std::string componentType = component->Name();
        auto parser = ComponentLoaders().find(componentType);
        if (parser == ComponentLoaders().end()) {
            LOG_WARNINGF("Scene", "Unknown component type: %s", componentType.c_str());
            return;
        }
        parser->second(component, world, entity, services);

        // Backward compatibility: a component's layerName attribute implies a LayerComponent.
        const char* layerName = component->Attribute("layerName");
        if (layerName && !world->HasComponent<LayerComponent>(entity)) {
            world->AddComponent<LayerComponent>(entity, LayerComponent(layerName));
        }
    });
}

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

void LoadEntities(XMLElement* root, World* world, ServiceLocator& services) {
    LOG_INFO("Scene", "Starting entity loading");

    ForEachElement(root, "Entities", [&](XMLElement* entities) {
        ForEachElement(entities, "Entity", [&](XMLElement* xmlEntity) {
            Entity entity = world->CreateEntity();
            LoadEntityComponents(xmlEntity, world, entity, services);
        });
    });
}

// After all entities are spawned, walk every ParentComponent.targetName,
// resolve it to an Entity ID, and populate the World's hierarchy adjacency.
void ResolveHierarchy(World* world) {
    // In entity (file) order: AddChild appends, so this sets sibling order.
    for (Entity child : std::vector<Entity>(world->GetLivingEntities())) {
        if (!world->HasComponent<ParentComponent>(child)) continue;
        auto& pc = world->GetComponent<ParentComponent>(child);
        if (pc.targetName.empty()) continue;
        // Already linked directly (intra-prefab parents resolve by local id).
        if (pc.parent != INVALID_ENTITY) continue;
        Entity parent = INVALID_ENTITY;
        if (world->GetEntityByName(pc.targetName, &parent)) {
            world->AddChild(parent, child);
        } else {
            LOG_WARNINGF("Scene", "Hierarchy: could not resolve parent name '%s' for entity %zu",
                         pc.targetName.c_str(), child);
        }
    }
}

void LoadSystems(XMLElement* root, Scene& scene) {
    VisitElement(root, "Systems", [&](XMLElement* systemsElement) {
        ForEachElement(systemsElement, "System", [&](XMLElement* xmlSystem) {
            const char* systemType = xmlSystem->Attribute("type");
            if (!systemType)
                return;

            std::string systemName = systemType;

            Context context = Context{.services = &scene.GetServices(), .scene = &scene, .world = scene.GetWorld()};

            auto system = SystemRegistry::Instance().Create(systemName, context);
            if (system) {
                // Every other attribute is a parameter, parsed as its declared type.
                const SystemParameters defaults = system->GetDefaultParameters();
                SystemParameters values;
                for (const XMLAttribute* attr = xmlSystem->FirstAttribute(); attr; attr = attr->Next()) {
                    std::string name = attr->Name();
                    if (name == "type") continue;
                    auto it = defaults.find(name);
                    if (it == defaults.end()) {
                        LOG_WARNINGF("Scene", "%s has no parameter '%s'", systemName.c_str(), name.c_str());
                        continue;
                    }
                    values[name] = Value::FromString(it->second.TypeName(), attr->Value());
                }
                system->Initialize(values);
                scene.AddSystem(std::move(system));
            }
        });
    });
}

std::string ScenePath(const std::string& name) { return Path("Scenes/" + name + ".xml").GetFullPath(); }

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
    scene.SetSource(std::filesystem::path(path).stem().string(), path);

    VisitElement(root, "EditorMetadata", [&](XMLElement* el) {
        for (const tinyxml2::XMLAttribute* a = el->FirstAttribute(); a; a = a->Next()) {
            scene.GetEditorMetadata()[a->Name()] = a->Value();
        }
    });

    LoadLayers(root, scene);
    LoadEntities(root, world_, scene.GetServices());
    PrefabInstances::Load(root, world_, DirectoryOf(path), scene.GetServices());
    ResolveHierarchy(world_);
    LoadSystems(root, scene);

    VisitElement(root, "SceneScript", [&](XMLElement* el) {
        const char* path = el->Attribute("path");
        if (path) {
            scene.SetSceneScript(path);
            auto& assetService = scene.GetServices().Get<Services::IAssetService>();
            assetService.LoadAsset<Script>(Path(path));
        }
    });

    // Allow subclasses to create additional custom systems
    scene.CreateCustomSystems();

    return true;
}
}  // namespace Elysium

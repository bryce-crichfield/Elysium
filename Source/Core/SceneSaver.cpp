#include <sstream>
#include <string>
#include <typeindex>
#include "Core/Components.h"
#include <unordered_map>
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
#include "Core/Xml.h"
#include "raylib.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium {

std::string ColorToHex(Color color) {
    if (color.a == 0)
        return "";
    char hex[9];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return std::string(hex);
}

using ComponentSaver = std::function<void(XMLBuilder&, World*, Entity)>;
const std::unordered_map<std::type_index, ComponentSaver>& ComponentSavers() {
    static std::unordered_map<std::type_index, ComponentSaver> componentSavers;

    if (!componentSavers.empty())
        return componentSavers;

    componentSavers[std::type_index(typeid(PositionComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& pos = world->GetComponent<PositionComponent>(entity);
        builder.AddElement("PositionComponent")
            .SetAttribute("x", pos.x)
            .SetAttribute("y", pos.y);
    };

    componentSavers[std::type_index(typeid(LocationComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& loc = world->GetComponent<LocationComponent>(entity);
        builder.AddElement("LocationComponent")
            .SetAttribute("x", loc.x)
            .SetAttribute("y", loc.y);
    };

    componentSavers[std::type_index(typeid(MovementComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& movement = world->GetComponent<MovementComponent>(entity);
        auto movementBuilder = builder.AddElement("MovementComponent");
        if (!movement.waypoints.empty()) {
            auto waypointsBuilder = movementBuilder.AddElement("Waypoints");
            for (const auto& waypoint : movement.waypoints) {
                waypointsBuilder.AddElement("Waypoint")
                    .SetAttribute("x", (int)waypoint.x)
                    .SetAttribute("y", (int)waypoint.y);
            }
        }
    };

    componentSavers[std::type_index(typeid(LayerComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& layer = world->GetComponent<LayerComponent>(entity);
        builder.AddElement("LayerComponent")
            .SetAttribute("name", layer.name.c_str());
    };

    componentSavers[std::type_index(typeid(RectangleComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& rect = world->GetComponent<RectangleComponent>(entity);
        auto rectBuilder = builder.AddElement("RectangleComponent")
                               .SetAttribute("width", rect.width)
                               .SetAttribute("height", rect.height);

        std::string bgHex = ColorToHex(rect.background);
        std::string borderHex = ColorToHex(rect.border);
        if (!bgHex.empty())
            rectBuilder.SetAttribute("background", bgHex.c_str());
        if (!borderHex.empty())
            rectBuilder.SetAttribute("border", borderHex.c_str());
    };

    componentSavers[std::type_index(typeid(CircleComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& circle = world->GetComponent<CircleComponent>(entity);
        auto circleBuilder = builder.AddElement("CircleComponent")
                                 .SetAttribute("radius", circle.radius);

        std::string fillHex = ColorToHex(circle.background);
        std::string borderHex = ColorToHex(circle.border);
        if (!fillHex.empty())
            circleBuilder.SetAttribute("fill", fillHex.c_str());
        if (!borderHex.empty())
            circleBuilder.SetAttribute("border", borderHex.c_str());
    };

    componentSavers[std::type_index(typeid(TextComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& text = world->GetComponent<TextComponent>(entity);
        builder.AddElement("TextComponent")
            .SetAttribute("text", text.content.c_str())
            .SetAttribute("fontSize", text.fontSize)
            .SetAttribute("r", text.color.r)
            .SetAttribute("g", text.color.g)
            .SetAttribute("b", text.color.b)
            .SetAttribute("a", text.color.a);
    };

    componentSavers[std::type_index(typeid(SpriteComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& sprite = world->GetComponent<SpriteComponent>(entity);
        builder.AddElement("SpriteComponent")
            .SetAttribute("spriteName", sprite.sprite.name.c_str())
            .SetAttribute("markerName", sprite.markerName.c_str());
    };

    componentSavers[std::type_index(typeid(CameraComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto cameraBuilder = builder.AddElement("CameraComponent");
        if (world->HasComponent<FollowComponent>(entity)) {
            auto& follow = world->GetComponent<FollowComponent>(entity);
            cameraBuilder.SetAttribute("target", follow.targetEntityName.c_str());
        }
    };

    componentSavers[std::type_index(typeid(LightComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& light = world->GetComponent<LightComponent>(entity);
        builder.AddElement("LightComponent")
            .SetAttribute("radius", light.radius)
            .SetAttribute("r", light.color.r)
            .SetAttribute("g", light.color.g)
            .SetAttribute("b", light.color.b)
            .SetAttribute("a", light.color.a);
    };

    componentSavers[std::type_index(typeid(AnimationComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        builder.AddElement("AnimationComponent");
    };

    componentSavers[std::type_index(typeid(DirectionComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        builder.AddElement("DirectionComponent");
    };

    componentSavers[std::type_index(typeid(TileComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        builder.AddElement("TileComponent");
    };

    componentSavers[std::type_index(typeid(ScriptComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& script = world->GetComponent<ScriptComponent>(entity);
        builder.AddElement("ScriptComponent")
            .SetAttribute("scriptName", script.scriptName.c_str());
    };

    componentSavers[std::type_index(typeid(KinematicsComponent))] = [](XMLBuilder& builder, World* world, Entity entity) {
        auto& kin = world->GetComponent<KinematicsComponent>(entity);
        builder.AddElement("KinematicsComponent")
            .SetAttribute("friction", kin.friction)
            .SetAttribute("maxSpeed", kin.maxSpeed);
    };

    return componentSavers;
}

std::string LayerSpaceToString(SceneLayerSpace space) {
    switch (space) {
        case SceneLayerSpace::Screen2D:
            return "Screen2D";
        case SceneLayerSpace::World2D:
            return "World2D";
        default:
            return "World2D";
    }
}

std::string LayerBlendToString(SceneLayerBlend blend) {
    switch (blend) {
        case SceneLayerBlend::Normal:
            return "Normal";
        case SceneLayerBlend::Additive:
            return "Additive";
        case SceneLayerBlend::Multiply:
            return "Multiply";
        case SceneLayerBlend::Alpha:
            return "Alpha";
        default:
            return "Normal";
    }
}

void SaveLayers(XMLBuilder& builder, const Scene& scene) {
    auto layersBuilder = builder.AddElement("Layers");

    for (const auto& layer : scene.GetLayers()) {
        layersBuilder.AddElement("Layer")
            .SetAttribute("name", layer.name.c_str())
            .SetAttribute("z", layer.zIndex)
            .SetAttribute("space", LayerSpaceToString(layer.space).c_str())
            .SetAttribute("layerBlend", LayerBlendToString(layer.layerBlend).c_str())
            .SetAttribute("compositeBlend", LayerBlendToString(layer.compositeBlend).c_str())
            .SetAttribute("opacity", layer.opacity)
            .SetAttribute("isVisible", layer.isVisible);
    }
}

void SaveTilemap(XMLBuilder& builder, World* world) {
    // Collect all tile entities and their definitions
    struct TileDef {
        RectangleComponent rect;
        std::string layerName;
    };
    std::unordered_map<int, TileDef> tileDefinitions;
    std::vector<int> tilemask;
    int maxX = 0, maxY = 0;

    world->Query<LocationComponent, TileComponent, RectangleComponent, LayerComponent>([&](Entity entity, const LocationComponent& loc, const TileComponent& tile, const RectangleComponent& rect, const LayerComponent& layer) {
        // Create a simple ID based on the rectangle properties
        int tileId = static_cast<int>(rect.width * 1000 + rect.height);  // Simple hash
        tileDefinitions[tileId] = {rect, layer.name};

        // Ensure tilemask is large enough
        int index = loc.y * (maxX + 1) + loc.x;
        if (index >= (int)tilemask.size()) {
            tilemask.resize(index + 1, 0);
        }
        tilemask[index] = tileId;

        maxX = std::max(maxX, loc.x);
        maxY = std::max(maxY, loc.y);
    });

    if (!tileDefinitions.empty()) {
        auto tilemapBuilder = builder.AddElement("Tilemap")
                                  .SetAttribute("width", maxX + 1)
                                  .SetAttribute("height", maxY + 1);

        // Save tile definitions
        auto tileDefsBuilder = tilemapBuilder.AddElement("TileDefinitions");
        for (const auto& [id, def] : tileDefinitions) {
            auto tileDefBuilder = tileDefsBuilder.AddElement("TileDefinition")
                                      .SetAttribute("id", id)
                                      .SetAttribute("layerName", def.layerName.c_str());

            std::string bgHex = ColorToHex(def.rect.background);
            std::string borderHex = ColorToHex(def.rect.border);
            if (!bgHex.empty())
                tileDefBuilder.SetAttribute("background", bgHex.c_str());
            if (!borderHex.empty())
                tileDefBuilder.SetAttribute("border", borderHex.c_str());
        }

        // Save tilemask
        auto tilemaskBuilder = tilemapBuilder.AddElement("Tilemask");
        std::ostringstream maskStream;
        for (size_t i = 0; i < tilemask.size(); ++i) {
            if (i > 0)
                maskStream << " ";
            maskStream << tilemask[i];
        }
        tilemaskBuilder.SetText(maskStream.str().c_str());
    }
}

void SaveEntities(XMLBuilder& builder, World* world) {
    auto entitiesBuilder = builder.AddElement("Entities");

    const auto& entities = world->GetLivingEntities();
    for (Entity entity : entities) {
        std::string entityName = world->GetEntityName(entity);

        // Skip layer entities and tile entities as they're saved separately
        if ((entityName.length() >= 6 && entityName.substr(0, 6) == "Layer_") || world->HasComponent<TileComponent>(entity)) {
            continue;
        }

        auto entityBuilder = entitiesBuilder.AddElement("Entity")
                                 .SetAttribute("name", entityName.c_str());

        // Check each component type individually
        const auto& savers = ComponentSavers();
        if (world->HasComponent<PositionComponent>(entity)) {
            savers.at(std::type_index(typeid(PositionComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<LocationComponent>(entity)) {
            savers.at(std::type_index(typeid(LocationComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<MovementComponent>(entity)) {
            savers.at(std::type_index(typeid(MovementComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<LayerComponent>(entity)) {
            savers.at(std::type_index(typeid(LayerComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<RectangleComponent>(entity)) {
            savers.at(std::type_index(typeid(RectangleComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<CircleComponent>(entity)) {
            savers.at(std::type_index(typeid(CircleComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<TextComponent>(entity)) {
            savers.at(std::type_index(typeid(TextComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<SpriteComponent>(entity)) {
            savers.at(std::type_index(typeid(SpriteComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<CameraComponent>(entity)) {
            savers.at(std::type_index(typeid(CameraComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<LightComponent>(entity)) {
            savers.at(std::type_index(typeid(LightComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<AnimationComponent>(entity)) {
            savers.at(std::type_index(typeid(AnimationComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<DirectionComponent>(entity)) {
            savers.at(std::type_index(typeid(DirectionComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<TileComponent>(entity)) {
            savers.at(std::type_index(typeid(TileComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<ScriptComponent>(entity)) {
            savers.at(std::type_index(typeid(ScriptComponent)))(entityBuilder, world, entity);
        }
        if (world->HasComponent<KinematicsComponent>(entity)) {
            savers.at(std::type_index(typeid(KinematicsComponent)))(entityBuilder, world, entity);
        }
    }
}

void SaveSystems(XMLBuilder& builder, const Scene& scene) {
    auto systemsBuilder = builder.AddElement("Systems");

    // Save active systems - this is a simplified approach
    // In a real implementation, you'd need to track which systems are active
    const auto& systems = scene.GetSystems();
    for (const auto& system : systems) {
        std::string systemName = system->GetName();
        systemsBuilder.AddElement("System")
            .SetAttribute("type", systemName.c_str());
    }
}

bool SaveScene(Scene& scene, const std::string& path) {
    LOG_INFOF("Scene", "Saving scene to XML: %s", path.c_str());
    XMLDocument doc;
    XMLElement* root = doc.NewElement("Scene");
    doc.InsertFirstChild(root);

    XMLBuilder builder(&doc, root);
    World* world = scene.GetWorld();

    SaveLayers(builder, scene);
    SaveTilemap(builder, world);
    SaveEntities(builder, world);
    SaveSystems(builder, scene);

    if (!SaveXml(path, doc)) {
        LOG_ERROR("Scene", "Failed to save scene file.");
        return false;
    }

    LOG_INFO("Scene", "Scene saved successfully");
    return true;
}
}  // namespace Elysium

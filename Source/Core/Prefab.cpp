#include "Core/Prefab.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <unordered_map>

#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/World.h"
#include "Core/Xml.h"
#include "Services/LogService.h"

using namespace tinyxml2;

namespace Elysium {

using ComponentLoader = std::function<void(XMLElement*, World*, Entity)>;

// Map of XML tag -> component parser function. Built once from the registry, with the
// CameraComponent override layered on top (matches the scene loader's historical
// behavior: a CameraComponent's "target" attribute also stamps a FollowComponent +
// ParentComponent so FollowSystem picks it up).
static const std::unordered_map<std::string, ComponentLoader>& ComponentLoaders() {
    static std::unordered_map<std::string, ComponentLoader> componentLoaders = [] {
        std::unordered_map<std::string, ComponentLoader> loaders;

        for (const auto& [name, loader] : ComponentRegistry::Instance().GetXmlLoaders()) {
            loaders[name] = loader;
        }

        loaders["CameraComponent"] = [](XMLElement* xmlComponent, World* world, Entity entity) {
            CameraComponent cam{};
            CameraComponent::LoadXml(cam, xmlComponent);
            world->AddComponent(entity, cam);

            std::string target = xmlComponent->Attribute("target") ? xmlComponent->Attribute("target") : "";
            if (!target.empty()) {
                world->AddComponent(entity, FollowComponent{});
                ParentComponent parentComp;
                parentComp.targetName = target;
                world->AddComponent(entity, parentComp);
            }
        };

        return loaders;
    }();

    return componentLoaders;
}

void LoadEntityComponents(XMLElement* xmlEntity, World* world, Entity entity) {
    ForEachChild(xmlEntity, [&](XMLElement* component) {
        std::string componentType = component->Name();
        auto parser = ComponentLoaders().find(componentType);
        if (parser != ComponentLoaders().end()) {
            parser->second(component, world, entity);

            const char* layerName = component->Attribute("layerName");
            if (layerName && !world->HasComponent<LayerComponent>(entity)) {
                world->AddComponent<LayerComponent>(entity, LayerComponent(layerName));
            }
        } else {
            LOG_WARNINGF("Prefab", "Unknown component type: %s", componentType.c_str());
        }
    });
}

bool LoadPrefabFile(const std::string& fullPath, XMLDocument& outDoc, XMLElement*& outEntitiesRoot) {
    outEntitiesRoot = nullptr;

    if (outDoc.LoadFile(fullPath.c_str()) != XML_SUCCESS) {
        LOG_ERRORF("Prefab", "Failed to load prefab file: %s", fullPath.c_str());
        return false;
    }

    outEntitiesRoot = outDoc.FirstChildElement("Entities");
    if (!outEntitiesRoot) {
        LOG_ERRORF("Prefab", "Prefab file missing root <Entities> element: %s", fullPath.c_str());
        return false;
    }

    return true;
}

PrefabIdMap LoadPrefabEntities(XMLElement* entitiesRoot, World* world, const std::string& instanceId) {
    PrefabIdMap idToEntity;
    int nextAutoId = 0;

    ForEachElement(entitiesRoot, "Entity", [&](XMLElement* xmlEntity) {
        int localId = xmlEntity->IntAttribute("id", nextAutoId);
        nextAutoId++;

        Entity entity = world->CreateEntity();
        LoadEntityComponents(xmlEntity, world, entity);

        if (!instanceId.empty() && world->HasComponent<NameComponent>(entity)) {
            auto& nameComp = world->GetComponent<NameComponent>(entity);
            nameComp.name = instanceId + "::" + nameComp.name;
        }

        idToEntity[localId] = entity;
    });

    // Resolve intra-prefab parent links directly via the local id map, bypassing
    // name-based lookup entirely so the namespacing above can't break them.
    for (auto& [localId, entity] : idToEntity) {
        if (!world->HasComponent<ParentComponent>(entity)) continue;

        auto& parentComp = world->GetComponent<ParentComponent>(entity);
        if (parentComp.targetName.empty()) continue;

        char* end = nullptr;
        long targetId = std::strtol(parentComp.targetName.c_str(), &end, 10);
        bool isNumeric = end != parentComp.targetName.c_str() && *end == '\0';
        if (isNumeric) {
            auto it = idToEntity.find(static_cast<int>(targetId));
            if (it != idToEntity.end()) {
                world->AddChild(it->second, entity);
            }
        }
        // Non-numeric or unresolved targets are left as-is for the caller's normal
        // by-name hierarchy resolution pass (cross-prefab / hand-authored references).
    }

    return idToEntity;
}

void ApplyPrefabOverride(World* world, const PrefabIdMap& idToEntity, int localId,
                          const std::string& component, const std::string& field, const std::string& value) {
    auto entityIt = idToEntity.find(localId);
    if (entityIt == idToEntity.end()) {
        LOG_WARNINGF("Prefab", "Dropping override for missing prefab entity id %d (component=%s field=%s)",
                     localId, component.c_str(), field.c_str());
        return;
    }

    const auto& support = ComponentRegistry::Instance().GetPrefabFieldSupport();
    auto supportIt = support.find(component);
    if (supportIt == support.end()) {
        LOG_WARNINGF("Prefab", "Unknown or non-overridable component '%s' in override for entity id %d",
                     component.c_str(), localId);
        return;
    }

    supportIt->second.applyOverride(world, entityIt->second, field, value);
}

bool SavePrefabFile(World* world, const std::string& fullPath) {
    XMLDocument doc;
    XMLElement* root = doc.NewElement("Entities");
    doc.InsertFirstChild(root);

    XMLBuilder builder(&doc, root);
    const auto& savers = ComponentRegistry::Instance().GetXmlSavers();

    int nextId = 0;
    for (Entity entity : world->GetLivingEntities()) {
        auto entityBuilder = builder.AddElement("Entity").SetAttribute("id", nextId++);
        for (const auto& [name, saver] : savers) {
            saver(entityBuilder, world, entity);
        }
    }

    return SaveXml(fullPath, doc);
}

std::vector<PrefabOverride> DiffPrefabEntity(int localId, World* liveWorld, Entity liveEntity,
                                              World* defaultWorld, Entity defaultEntity) {
    std::vector<PrefabOverride> overrides;
    const auto& support = ComponentRegistry::Instance().GetPrefabFieldSupport();

    for (const auto& [xmlTag, fieldSupport] : support) {
        XMLDocument liveDoc;
        XMLElement* liveElem = fieldSupport.serialize(liveDoc, liveWorld, liveEntity);

        XMLDocument defaultDoc;
        XMLElement* defaultElem = fieldSupport.serialize(defaultDoc, defaultWorld, defaultEntity);

        if (!liveElem && !defaultElem) continue;
        if (!liveElem || !defaultElem) {
            // Structural difference (component present on only one side) — not
            // representable as field-level overrides. See the design doc's
            // "detach from prefab" open question.
            continue;
        }

        std::set<std::string> attrNames;
        for (const XMLAttribute* a = liveElem->FirstAttribute(); a; a = a->Next()) attrNames.insert(a->Name());
        for (const XMLAttribute* a = defaultElem->FirstAttribute(); a; a = a->Next()) attrNames.insert(a->Name());

        for (const auto& attrName : attrNames) {
            const char* liveVal = liveElem->Attribute(attrName.c_str());
            const char* defaultVal = defaultElem->Attribute(attrName.c_str());
            std::string lv = liveVal ? liveVal : "";
            std::string dv = defaultVal ? defaultVal : "";
            if (lv != dv) {
                overrides.push_back(PrefabOverride{localId, xmlTag, attrName, lv});
            }
        }
    }

    // Stable, deterministic ordering so saved XML doesn't churn on unordered_map iteration.
    std::sort(overrides.begin(), overrides.end(), [](const PrefabOverride& a, const PrefabOverride& b) {
        if (a.component != b.component) return a.component < b.component;
        return a.field < b.field;
    });

    return overrides;
}

}  // namespace Elysium

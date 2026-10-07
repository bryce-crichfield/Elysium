#include "Core/PrefabInstance.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>

#include "Core/Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Log.h"
#include "Core/Scene.h"
#include "Core/World.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"

using namespace tinyxml2;

namespace Elysium::PrefabInstances {

namespace {

std::string Attr(XMLElement* el, const char* name) {
    const char* value = el->Attribute(name);
    return value ? value : "";
}

void SetOrAdd(World* world, Entity entity, const PrefabInstanceComponent& component) {
    if (world->HasComponent<PrefabInstanceComponent>(entity)) {
        world->GetComponent<PrefabInstanceComponent>(entity) = component;
    } else {
        world->AddComponent<PrefabInstanceComponent>(entity, component);
    }
}

// Attribute differences between `live` and `def`, recursing into nested elements that
// exist on both sides (matched by FieldKey); `prefix` is the path down to them.
void DiffElement(int localId, const std::string& component, XMLElement* live, XMLElement* def,
                 const std::string& prefix, std::vector<PrefabOverride>& overrides) {
    std::set<std::string> attrNames;
    for (const XMLAttribute* a = live->FirstAttribute(); a; a = a->Next()) attrNames.insert(a->Name());
    for (const XMLAttribute* a = def->FirstAttribute(); a; a = a->Next()) attrNames.insert(a->Name());
    for (const auto& attrName : attrNames) {
        const std::string liveVal = Attr(live, attrName.c_str()), defaultVal = Attr(def, attrName.c_str());
        if (liveVal != defaultVal) overrides.push_back({localId, component, prefix + attrName, liveVal});
    }
    for (XMLElement* child = live->FirstChildElement(); child; child = child->NextSiblingElement()) {
        const std::string key = FieldKey(child);
        if (XMLElement* defChild = FindChildByKey(def, key)) {
            DiffElement(localId, component, child, defChild, prefix + key + "/", overrides);
        }
    }
}

// Field-level differences between a live entity and its prefab default, reusing each
// component's XmlSaver as the diffing substrate. Components present on only one side
// aren't representable as field overrides and are skipped.
std::vector<PrefabOverride> DiffEntity(int localId, World* liveWorld, Entity liveEntity, World* defaultWorld,
                                       Entity defaultEntity) {
    std::vector<PrefabOverride> overrides;

    for (const auto& [xmlTag, fieldSupport] : ComponentRegistry::Instance().GetPrefabFieldSupport()) {
        XMLDocument liveDoc, defaultDoc;
        XMLElement* liveElem = fieldSupport.serialize(liveDoc, liveWorld, liveEntity);
        XMLElement* defaultElem = fieldSupport.serialize(defaultDoc, defaultWorld, defaultEntity);
        if (!liveElem || !defaultElem) continue;

        DiffElement(localId, xmlTag, liveElem, defaultElem, "", overrides);
    }

    std::sort(overrides.begin(), overrides.end(), [](const PrefabOverride& a, const PrefabOverride& b) {
        return a.component != b.component ? a.component < b.component : a.field < b.field;
    });
    return overrides;
}

// Values stored in NameComponent are namespaced by instance ("Knight1::Unit"); strip it for params/overrides.
std::string StripNamespace(const std::string& value, const std::string& instanceId) {
    const std::string prefix = instanceId + "::";
    return value.rfind(prefix, 0) == 0 ? value.substr(prefix.size()) : value;
}

}  // namespace

std::vector<Entity> Load(XMLElement* parent, World* world, const std::string& ownerDir, ServiceLocator& services,
                         const std::string& namePrefix) {
    std::vector<Entity> spawned;

    ForEachElement(parent, "PrefabInstance", [&](XMLElement* xmlInstance) {
        const std::string src = Attr(xmlInstance, "src");
        const std::string id = Attr(xmlInstance, "id");
        if (src.empty() || id.empty()) {
            LOG_ERROR("Prefab", "PrefabInstance requires both 'src' and 'id'; skipping.");
            return;
        }

        const Prefab* prefab = Prefab::Get(services, ownerDir + src);
        if (!prefab) return;

        const std::string instanceId = namePrefix + id;
        PrefabSpawnResult result = prefab->Spawn(world, instanceId, services);

        // Tag everything, including entities of nested instances: the outermost placement
        // owns them, and nested entities (not in `ids`) get localEntityId -1 so they're
        // recreated from the file rather than diffed.
        for (Entity entity : result.spawned) {
            PrefabInstanceComponent tag;
            tag.src = src;
            tag.ownerDir = ownerDir;
            tag.instanceId = instanceId;
            for (const auto& [localId, e] : result.ids) {
                if (e == entity) tag.localEntityId = localId;
            }
            SetOrAdd(world, entity, tag);
        }

        auto apply = [&](PrefabOverride ov) { ApplyOverride(world, result.ids, ov, services); };

        // A parameter name may drive several fields (e.g. X -> transform x and movement goalX).
        ForEachElement(xmlInstance, "Param", [&](XMLElement* xmlParam) {
            const std::string name = Attr(xmlParam, "name");
            if (!prefab->FindParameter(name)) {
                LOG_WARNINGF("Prefab", "'%s' has no parameter '%s'", src.c_str(), name.c_str());
                return;
            }
            for (const auto& param : prefab->GetParameters()) {
                if (param.name == name) apply({param.entity, param.component, param.field, Attr(xmlParam, "value")});
            }
        });

        ForEachElement(xmlInstance, "Override", [&](XMLElement* xmlOverride) {
            apply({xmlOverride->IntAttribute("entity", -1), Attr(xmlOverride, "component"),
                   Attr(xmlOverride, "field"), Attr(xmlOverride, "value")});
        });

        spawned.insert(spawned.end(), result.spawned.begin(), result.spawned.end());
    });

    return spawned;
}

void Save(XMLBuilder& builder, World* world, ServiceLocator& services, const Scene* host, const Filter& filter) {
    struct InstanceGroup {
        std::string src;
        std::string ownerDir;
        std::vector<Entity> entities;
    };
    std::map<std::string, InstanceGroup> groups;  // ordered by id for stable output

    for (Entity entity : world->GetLivingEntities()) {
        if (!world->HasComponent<PrefabInstanceComponent>(entity)) continue;
        const auto& tag = world->GetComponent<PrefabInstanceComponent>(entity);
        if (filter && !filter(tag)) continue;
        auto& group = groups[tag.instanceId];
        group.src = tag.src;
        group.ownerDir = tag.ownerDir;
        group.entities.push_back(entity);
    }

    auto& assets = services.Get<Services::IAssetService>();
    for (const auto& [instanceId, group] : groups) {
        auto instanceBuilder = builder.AddElement("PrefabInstance")
                                   .SetAttribute("src", group.src.c_str())
                                   .SetAttribute("id", instanceId.c_str());

        const Prefab* prefab = Prefab::Get(assets, group.ownerDir + group.src);
        if (!prefab) {
            LOG_ERRORF("Prefab", "Saving '%s' without overrides: could not load '%s'", instanceId.c_str(), group.src.c_str());
            continue;
        }

        // Defaults spawned with the same instance id, so namespaced names compare equal, and
        // settled through the same paused systems the live entities have been through.
        Scene scratch(services);
        if (host) scratch.CopySetupFrom(*host, true);
        PrefabSpawnResult defaults = prefab->Spawn(scratch.GetWorld(), instanceId, services);
        if (host) scratch.OnUpdate(0.0f, false);

        std::vector<PrefabOverride> overrides;
        for (Entity liveEntity : group.entities) {
            const int localId = world->GetComponent<PrefabInstanceComponent>(liveEntity).localEntityId;
            if (localId < 0) continue;  // entity of a nested instance
            auto defaultIt = defaults.ids.find(localId);
            if (defaultIt == defaults.ids.end()) continue;
            auto diff = DiffEntity(localId, world, liveEntity, scratch.GetWorld(), defaultIt->second);
            // Black box: a placement only owns its root's placement data (name, transform);
            // everything else changes through exposed parameters.
            const bool isRoot = IsRoot(*world, liveEntity);
            const auto& registry = ComponentRegistry::Instance();
            for (auto& ov : diff) {
                const bool owned = isRoot && registry.IsPlacementOwned(ov.component);
                if (owned || prefab->FindParameter(ov.entity, ov.component, ov.field)) overrides.push_back(std::move(ov));
            }
        }

        // Exposed parameters are written once by name; everything else as a raw override.
        std::set<std::string> writtenParams;
        for (auto& ov : overrides) {
            if (ov.component == "NameComponent" && ov.field == "name") ov.value = StripNamespace(ov.value, instanceId);
            const PrefabParameter* param = prefab->FindParameter(ov.entity, ov.component, ov.field);
            if (param && writtenParams.insert(param->name).second) {
                instanceBuilder.AddElement("Param")
                    .SetAttribute("name", param->name.c_str())
                    .SetAttribute("value", ov.value.c_str());
            }
        }
        for (const auto& ov : overrides) {
            if (prefab->FindParameter(ov.entity, ov.component, ov.field)) continue;
            instanceBuilder.AddElement("Override")
                .SetAttribute("entity", ov.entity)
                .SetAttribute("component", ov.component.c_str())
                .SetAttribute("field", ov.field.c_str())
                .SetAttribute("value", ov.value.c_str());
        }
    }
}

void ApplyOverride(World* world, const PrefabIdMap& idToEntity, const PrefabOverride& ov, ServiceLocator& services) {
    auto entityIt = idToEntity.find(ov.entity);
    if (entityIt == idToEntity.end()) {
        LOG_WARNINGF("Prefab", "Dropping override for missing prefab entity id %d (%s.%s)",
                     ov.entity, ov.component.c_str(), ov.field.c_str());
        return;
    }

    const auto& support = ComponentRegistry::Instance().GetPrefabFieldSupport();
    auto supportIt = support.find(ov.component);
    if (supportIt == support.end()) {
        LOG_WARNINGF("Prefab", "Component '%s' can't be overridden", ov.component.c_str());
        return;
    }
    supportIt->second.applyOverride(world, entityIt->second, ov.field, ov.value, services);
}

std::string ReadField(World* world, Entity entity, const std::string& component, const std::string& field) {
    const auto& support = ComponentRegistry::Instance().GetPrefabFieldSupport();
    auto it = support.find(component);
    if (it == support.end()) return "";
    XMLDocument scratch;
    XMLElement* el = it->second.serialize(scratch, world, entity);
    std::string attr;
    XMLElement* target = el ? ResolveField(el, field, attr) : nullptr;
    const char* value = target ? target->Attribute(attr.c_str()) : nullptr;
    return value ? value : "";
}

bool IsRoot(const World& world, Entity entity) {
    if (!world.HasComponent<PrefabInstanceComponent>(entity)) return false;
    const Entity parent = world.GetParent(entity);
    return parent == INVALID_ENTITY || !world.HasComponent<PrefabInstanceComponent>(parent) ||
           world.GetComponent<PrefabInstanceComponent>(parent).instanceId !=
               world.GetComponent<PrefabInstanceComponent>(entity).instanceId;
}

bool IsInternal(const World& world, Entity entity) {
    return world.HasComponent<PrefabInstanceComponent>(entity) && !IsRoot(world, entity);
}

void Reinstance(World* world, const std::vector<Entity>& entities) {
    std::set<std::string> taken;
    world->Query<PrefabInstanceComponent>([&](Entity, PrefabInstanceComponent& tag) { taken.insert(tag.instanceId); });

    // Only the outermost placements are remapped; a nested instance's id carries its parent's as
    // a prefix ("Outer1::Inner1"), so renaming the outer one carries the nested ones with it.
    std::map<std::string, std::string> renamed;
    for (Entity entity : entities) {
        if (!IsRoot(*world, entity)) continue;
        const std::string& old = world->GetComponent<PrefabInstanceComponent>(entity).instanceId;
        if (old.empty() || renamed.count(old)) continue;

        // Count up from the prefab's own name rather than the old id, so a copy of "Floor7" is
        // "Floor12" and not "Floor71".
        const std::string base = std::filesystem::path(world->GetComponent<PrefabInstanceComponent>(entity).src).stem().string();
        std::string fresh;
        for (int n = 1; fresh.empty() || taken.count(fresh); ++n) fresh = base + std::to_string(n);
        taken.insert(fresh);
        renamed[old] = fresh;
    }
    if (renamed.empty()) return;

    // A prefix rewrite, so it reaches an entity's id, its name, and any nested instance under it.
    auto rewrite = [](const std::string& value, const std::string& from, const std::string& to) {
        if (value == from) return to;
        if (value.rfind(from + "::", 0) == 0) return to + value.substr(from.size());
        return value;
    };

    for (Entity entity : entities) {
        if (!world->HasComponent<PrefabInstanceComponent>(entity)) continue;
        auto& tag = world->GetComponent<PrefabInstanceComponent>(entity);
        for (const auto& [from, to] : renamed) {
            tag.instanceId = rewrite(tag.instanceId, from, to);
            if (world->HasComponent<NameComponent>(entity)) {
                auto& name = world->GetComponent<NameComponent>(entity).name;
                name = rewrite(name, from, to);
            }
        }
    }
}

Entity RootOf(const World& world, Entity entity) {
    while (entity != INVALID_ENTITY && IsInternal(world, entity)) entity = world.GetParent(entity);
    return entity;
}

}  // namespace Elysium::PrefabInstances

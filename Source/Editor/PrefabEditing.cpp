#include "Editor/PrefabEditing.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <set>

#include "Core/Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Path.h"
#include "Core/World.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"

using namespace tinyxml2;

namespace Elysium::PrefabEditing {

namespace {

std::string Attr(XMLElement* el, const char* name) {
    const char* value = el->Attribute(name);
    return value ? value : "";
}

// Loads one <PrefabInstance> element (cloned into a scratch host) into `world`, resolving
// any by-name parents the prefab refers to within that world.
std::vector<Entity> LoadOne(XMLElement* xmlInstance, World* world, const std::string& ownerDir, ServiceLocator& services) {
    XMLDocument doc;
    XMLElement* host = doc.NewElement("Host");
    doc.InsertFirstChild(host);
    host->InsertEndChild(xmlInstance->DeepClone(&doc));

    auto spawned = PrefabInstances::Load(host, world, ownerDir, services);
    for (Entity entity : spawned) {
        if (!world->HasComponent<ParentComponent>(entity)) continue;
        auto& pc = world->GetComponent<ParentComponent>(entity);
        Entity parent = INVALID_ENTITY;
        if (pc.parent == INVALID_ENTITY && !pc.targetName.empty() && world->GetEntityByName(pc.targetName, &parent)) {
            world->AddChild(parent, entity);
        }
    }
    return spawned;
}

// Writes a <Prefab> file from `own` (the prefab's own entities, by local id) plus the
// placements selected by `placements`, whose `src`s are rewritten relative to the new file.
// A parent that is neither an own entity nor a placement is outside the prefab and dropped.
bool WritePrefab(World* world, const std::string& fullPath, const std::vector<Entity>& own,
                 const std::unordered_map<Entity, int>& localIds, const std::vector<PrefabParameter>& parameters,
                 ServiceLocator& services, const Scene* host, const PrefabInstances::Filter& placements) {
    XMLDocument doc;
    XMLElement* root = doc.NewElement("Prefab");
    doc.InsertFirstChild(root);
    XMLBuilder builder(&doc, root);

    if (!parameters.empty()) {
        auto paramsBuilder = builder.AddElement("Parameters");
        for (const auto& param : parameters) {
            paramsBuilder.AddElement("Parameter")
                .SetAttribute("name", param.name.c_str())
                .SetAttribute("entity", param.entity)
                .SetAttribute("component", param.component.c_str())
                .SetAttribute("field", param.field.c_str());
        }
    }

    const auto& savers = ComponentRegistry::Instance().GetXmlSavers();
    auto entitiesBuilder = builder.AddElement("Entities");
    for (Entity entity : own) {
        auto entityBuilder = entitiesBuilder.AddElement("Entity").SetAttribute("id", localIds.at(entity));
        for (const auto& [name, saver] : savers) saver(entityBuilder, world, entity);

        // Parents inside this prefab are referenced by local id, so they survive renames.
        if (world->HasComponent<ParentComponent>(entity)) {
            const Entity parent = world->GetComponent<ParentComponent>(entity).parent;
            auto parentIt = localIds.find(parent);
            const bool outside = parentIt == localIds.end() && !world->HasComponent<PrefabInstanceComponent>(parent);
            XMLElement* entityEl = entityBuilder.GetElement();
            for (XMLElement* el = entityEl->FirstChildElement("ParentComponent"); el;) {
                XMLElement* next = el->NextSiblingElement("ParentComponent");
                if (parentIt != localIds.end()) el->SetAttribute("target", parentIt->second);
                else if (outside) entityEl->DeleteChild(el);
                el = next;
            }
        }
    }

    PrefabInstances::Save(builder, world, services, host, placements);

    // Placements' `src` is relative to where they were placed; make it relative to this file.
    std::unordered_map<std::string, std::string> ownerDirs;
    world->Query<PrefabInstanceComponent>([&](Entity, PrefabInstanceComponent& tag) { ownerDirs[tag.instanceId] = tag.ownerDir; });
    for (XMLElement* el = root->FirstChildElement("PrefabInstance"); el; el = el->NextSiblingElement("PrefabInstance")) {
        const std::string prefabPath = ownerDirs[Attr(el, "id")] + Attr(el, "src");
        el->SetAttribute("src", RelativeTo(prefabPath, DirectoryOf(fullPath)).c_str());
    }

    if (!SaveXml(fullPath, doc)) return false;

    // Any cached copy is now stale; placements spawned from here on use the new file.
    return Prefab::Reload(services.Get<Services::IAssetService>(), fullPath) != nullptr;
}

}  // namespace

std::unique_ptr<XMLDocument> Snapshot(World* world, ServiceLocator& services, const Scene* host,
                                      const PrefabInstances::Filter& filter) {
    auto snapshot = std::make_unique<XMLDocument>();
    XMLElement* root = snapshot->NewElement("Snapshot");
    snapshot->InsertFirstChild(root);
    XMLBuilder builder(snapshot.get(), root);
    PrefabInstances::Save(builder, world, services, host, filter);

    // Remember each placement's owner directory, which the XML form leaves implicit.
    std::unordered_map<std::string, std::string> ownerDirs;
    world->Query<PrefabInstanceComponent>([&](Entity, PrefabInstanceComponent& tag) { ownerDirs[tag.instanceId] = tag.ownerDir; });
    for (XMLElement* el = root->FirstChildElement("PrefabInstance"); el; el = el->NextSiblingElement("PrefabInstance")) {
        el->SetAttribute("ownerDir", ownerDirs[Attr(el, "id")].c_str());
    }
    return snapshot;
}

void Respawn(World* world, XMLDocument& snapshot, ServiceLocator& services, const PrefabInstances::Filter& filter) {
    // Outside entities parented to a placement's root, to re-attach to the new root.
    std::unordered_map<std::string, std::vector<Entity>> outsideChildren;
    std::vector<Entity> doomed;
    for (Entity entity : world->GetLivingEntities()) {
        if (!world->HasComponent<PrefabInstanceComponent>(entity)) continue;
        const auto& tag = world->GetComponent<PrefabInstanceComponent>(entity);
        if (filter && !filter(tag)) continue;
        doomed.push_back(entity);
        if (!PrefabInstances::IsRoot(*world, entity)) continue;
        for (Entity child : world->GetChildren(entity)) {
            if (!world->HasComponent<PrefabInstanceComponent>(child) ||
                world->GetComponent<PrefabInstanceComponent>(child).instanceId != tag.instanceId) {
                outsideChildren[tag.instanceId].push_back(child);
            }
        }
    }
    for (Entity entity : doomed) world->DestroyEntity(entity);

    XMLElement* root = snapshot.RootElement();
    if (!root) return;
    for (XMLElement* el = root->FirstChildElement("PrefabInstance"); el; el = el->NextSiblingElement("PrefabInstance")) {
        // One at a time: each placement resolves `src` against its own owner directory.
        auto spawned = LoadOne(el, world, Attr(el, "ownerDir"), services);
        if (spawned.empty()) continue;
        const Entity newRoot = PrefabInstances::RootOf(*world, spawned.front());
        for (Entity child : outsideChildren[Attr(el, "id")]) world->AddChild(newRoot, child);
    }
}

std::string UniqueInstanceId(World* world, const std::string& base) {
    std::set<std::string> taken;
    world->Query<PrefabInstanceComponent>([&](Entity, PrefabInstanceComponent& tag) { taken.insert(tag.instanceId); });
    std::string id;
    for (int n = 1; id.empty() || taken.count(id); ++n) id = base + std::to_string(n);
    return id;
}

Entity Instantiate(World* world, const std::string& fullPath, const std::string& ownerDir, ServiceLocator& services) {
    // Go through the XML path so the placement behaves exactly like a loaded one.
    XMLDocument doc;
    XMLElement* xmlInstance = doc.NewElement("PrefabInstance");
    xmlInstance->SetAttribute("src", RelativeTo(fullPath, ownerDir).c_str());
    xmlInstance->SetAttribute("id", UniqueInstanceId(world, std::filesystem::path(fullPath).stem().string()).c_str());
    doc.InsertFirstChild(xmlInstance);

    auto spawned = LoadOne(xmlInstance, world, ownerDir, services);
    return spawned.empty() ? INVALID_ENTITY : spawned.front();
}

bool SaveFile(World* world, const std::string& fullPath, std::unordered_map<Entity, int>& localIds,
              const std::vector<PrefabParameter>& parameters, ServiceLocator& services, const Scene* host) {
    // Own entities first get ids: keep existing ones, hand out fresh ones above the max.
    std::vector<Entity> own;
    for (Entity entity : world->GetLivingEntities()) {
        if (!world->HasComponent<PrefabInstanceComponent>(entity)) own.push_back(entity);
    }
    // Forget deleted entities: their Entity values can be recycled for new ones.
    std::erase_if(localIds, [&](const auto& entry) { return std::find(own.begin(), own.end(), entry.first) == own.end(); });
    int nextId = 0;
    for (const auto& [entity, id] : localIds) nextId = std::max(nextId, id + 1);
    for (Entity entity : own) {
        if (!localIds.count(entity)) localIds[entity] = nextId++;
    }

    return WritePrefab(world, fullPath, own, localIds, parameters, services, host, {});
}

void Unpack(World* world, Entity root) {
    if (!PrefabInstances::IsRoot(*world, root)) return;
    const std::string instanceId = world->GetComponent<PrefabInstanceComponent>(root).instanceId;
    std::vector<Entity> members;
    for (Entity entity : world->GetLivingEntities()) {
        if (world->HasComponent<PrefabInstanceComponent>(entity) &&
            world->GetComponent<PrefabInstanceComponent>(entity).instanceId == instanceId) {
            members.push_back(entity);
        }
    }
    // Plain entities are parented by name (a prefab's by local id): every member gets a
    // unique name, then its children point at it.
    for (Entity entity : members) {
        if (world->GetEntityName(entity).empty()) {
            const int localId = world->GetComponent<PrefabInstanceComponent>(entity).localEntityId;
            world->AddComponent(entity, NameComponent(instanceId + "::Entity" + std::to_string(localId >= 0 ? localId : (int)entity)));
        }
    }
    for (Entity entity : members) {
        const Entity parent = world->GetParent(entity);
        if (parent != INVALID_ENTITY && world->HasComponent<ParentComponent>(entity)) {
            world->GetComponent<ParentComponent>(entity).targetName = world->GetEntityName(parent);
        }
    }
    for (Entity entity : members) world->RemoveComponent<PrefabInstanceComponent>(entity);
}

bool SaveSubtree(World* world, Entity root, const std::string& fullPath, ServiceLocator& services, const Scene* host) {
    // The subtree's own entities in depth-first order (root first, as id 0), and the
    // placements inside it, which a placement's root stands in for.
    std::vector<Entity> own;
    std::unordered_map<Entity, int> localIds;
    std::set<std::string> placements;
    std::function<void(Entity)> collect = [&](Entity entity) {
        if (world->HasComponent<PrefabInstanceComponent>(entity)) {
            placements.insert(world->GetComponent<PrefabInstanceComponent>(entity).instanceId);
        } else {
            localIds[entity] = (int)own.size();
            own.push_back(entity);
        }
        // A placement's internals come from its own prefab file; entities parented to it
        // from outside are ours.
        for (Entity child : world->GetChildren(entity)) {
            if (!PrefabInstances::IsInternal(*world, child)) collect(child);
        }
    };
    if (world->HasComponent<PrefabInstanceComponent>(root)) return false;  // already a placement
    collect(root);

    return WritePrefab(world, fullPath, own, localIds, {}, services, host,
                       [&](const PrefabInstanceComponent& tag) { return placements.count(tag.instanceId) > 0; });
}

}  // namespace Elysium::PrefabEditing

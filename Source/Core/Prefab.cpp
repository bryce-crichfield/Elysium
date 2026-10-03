#include "Core/Prefab.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <set>

#include "Core/Components.h"
#include "Core/Log.h"
#include "Core/Path.h"
#include "Core/PrefabInstance.h"
#include "Core/Scene.h"
#include "Core/World.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"

using namespace tinyxml2;

namespace Elysium {

namespace {

std::string Attr(XMLElement* el, const char* name) {
    const char* value = el->Attribute(name);
    return value ? value : "";
}

bool ParseInt(const std::string& text, int& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    long value = std::strtol(text.c_str(), &end, 10);
    if (*end != '\0') return false;
    out = static_cast<int>(value);
    return true;
}

// Nesting depth of Spawn calls on this thread, to stop a prefab cycle (A places B places A).
thread_local int spawnDepth = 0;
constexpr int kMaxSpawnDepth = 16;

}  // namespace

const Prefab* Prefab::Get(Services::IAssetService& assets, const std::string& fullPath) {
    return assets.LoadAssetNow<Prefab>(Path::FromFullPath(fullPath));
}

const Prefab* Prefab::Reload(Services::IAssetService& assets, const std::string& fullPath) {
    return assets.ReloadAssetNow<Prefab>(Path::FromFullPath(fullPath));
}

bool Prefab::Load(const std::string& fullPath) {
    fullPath_ = fullPath;
    parameters_.clear();
    root_ = entities_ = nullptr;

    if (doc_.LoadFile(fullPath.c_str()) != XML_SUCCESS) {
        LOG_ERRORF("Prefab", "Failed to load prefab file: %s", fullPath.c_str());
        return false;
    }
    root_ = doc_.RootElement();
    if (!root_ || std::string(root_->Name()) != "Prefab") {
        LOG_ERRORF("Prefab", "Prefab root must be <Prefab>: %s", fullPath.c_str());
        root_ = nullptr;
        return false;
    }

    entities_ = root_->FirstChildElement("Entities");
    VisitElement(root_, "Parameters", [&](XMLElement* xmlParams) {
        ForEachElement(xmlParams, "Parameter", [&](XMLElement* xmlParam) {
            PrefabParameter param;
            param.name = Attr(xmlParam, "name");
            param.entity = xmlParam->IntAttribute("entity", 0);
            param.component = Attr(xmlParam, "component");
            param.field = Attr(xmlParam, "field");
            if (!param.name.empty() && !param.component.empty() && !param.field.empty()) {
                parameters_.push_back(std::move(param));
            }
        });
    });
    return true;
}

const PrefabParameter* Prefab::FindParameter(const std::string& name) const {
    for (const auto& p : parameters_) if (p.name == name) return &p;
    return nullptr;
}

const PrefabParameter* Prefab::FindParameter(int entity, const std::string& component, const std::string& field) const {
    for (const auto& p : parameters_) {
        if (p.entity == entity && p.component == component && p.field == field) return &p;
    }
    return nullptr;
}

PrefabSpawnResult Prefab::Spawn(World* world, const std::string& instanceId, ServiceLocator& services) const {
    PrefabSpawnResult result;
    if (spawnDepth >= kMaxSpawnDepth) {
        LOG_ERRORF("Prefab", "%s: prefabs nested too deeply (does it place itself?); skipped", fullPath_.c_str());
        return result;
    }

    if (entities_) {
        int nextAutoId = 0;
        ForEachElement(entities_, "Entity", [&](XMLElement* xmlEntity) {
            int localId = xmlEntity->IntAttribute("id", nextAutoId);
            nextAutoId = std::max(nextAutoId, localId) + 1;

            Entity entity = world->CreateEntity();
            LoadEntityComponents(xmlEntity, world, entity, services);

            if (!instanceId.empty() && world->HasComponent<NameComponent>(entity)) {
                auto& nameComp = world->GetComponent<NameComponent>(entity);
                nameComp.name = instanceId + "::" + nameComp.name;
            }

            result.ids[localId] = entity;
            result.spawned.push_back(entity);
        });
    }

    // Intra-prefab parent links by local id, independent of (namespaced) names. In file
    // order (`spawned` holds only the direct entities so far): AddChild appends, so this
    // is what sets sibling order. (`ids` is unordered.)
    for (Entity entity : std::vector<Entity>(result.spawned)) {
        if (!world->HasComponent<ParentComponent>(entity)) continue;
        int targetId = 0;
        if (!ParseInt(world->GetComponent<ParentComponent>(entity).targetName, targetId)) continue;
        auto it = result.ids.find(targetId);
        if (it != result.ids.end()) world->AddChild(it->second, entity);
    }

    // A prefab has exactly one root: <Entity id="0"> if present, else the first. Any other
    // entity left without a parent competes with it and is rejected, with its subtree. (An
    // unresolved by-name target is a link outside the prefab, left for the caller.)
    if (!result.ids.empty()) {
        const Entity root = result.ids.count(0) ? result.ids.at(0) : result.spawned.front();

        std::vector<std::pair<int, Entity>> rejected;
        for (const auto& [localId, entity] : result.ids) {
            if (entity == root || world->GetParent(entity) != INVALID_ENTITY) continue;
            int unused = 0;
            const bool linksOutside = world->HasComponent<ParentComponent>(entity) &&
                                      !world->GetComponent<ParentComponent>(entity).targetName.empty() &&
                                      !ParseInt(world->GetComponent<ParentComponent>(entity).targetName, unused);
            if (!linksOutside) rejected.emplace_back(localId, entity);
        }
        for (const auto& [localId, entity] : rejected) {
            LOG_ERRORF("Prefab", "%s: entity id %d competes with the root (a prefab has one root); rejected",
                       fullPath_.c_str(), localId);
            const std::vector<Entity> doomed = world->GetSubtree(entity);
            for (auto it = doomed.rbegin(); it != doomed.rend(); ++it) {
                std::erase_if(result.ids, [&](const auto& entry) { return entry.second == *it; });
                std::erase(result.spawned, *it);
                world->DestroyEntity(*it);
            }
        }
    }

    // The root carries the placement's own name; the prefab it came from is shown by the
    // editor, not stored in the name. Internal entities stay namespaced ("Unit1::Body") so
    // two placements' by-name references don't collide.
    if (!instanceId.empty() && !result.ids.empty()) {
        const Entity root = result.ids.count(0) ? result.ids.at(0) : result.spawned.front();
        if (world->HasComponent<NameComponent>(root)) world->GetComponent<NameComponent>(root).name = instanceId;
        else world->AddComponent(root, NameComponent(instanceId));
    }

    // Composition: prefabs placed inside this prefab.
    if (root_) {
        ++spawnDepth;
        auto nested = PrefabInstances::Load(root_, world, DirectoryOf(fullPath_), services,
                                            instanceId.empty() ? "" : instanceId + "::");
        --spawnDepth;
        result.spawned.insert(result.spawned.end(), nested.begin(), nested.end());

        // Nested placements with no parent of their own hang under this prefab's root, so
        // they move with it and stay inside its black box.
        if (!result.ids.empty()) {
            const Entity root = result.ids.count(0) ? result.ids.at(0) : result.spawned.front();
            for (Entity entity : nested) {
                const bool hasParent = world->HasComponent<ParentComponent>(entity) &&
                                       (world->GetParent(entity) != INVALID_ENTITY ||
                                        !world->GetComponent<ParentComponent>(entity).targetName.empty());
                if (!hasParent) world->AddChild(root, entity);
            }
        }
    }

    return result;
}

bool Prefab::DependsOn(const std::string& fullPath, Services::IAssetService& assets) const {
    std::set<std::string> visited;
    std::function<bool(const Prefab&)> visit = [&](const Prefab& prefab) {
        if (SamePath(prefab.fullPath_, fullPath)) return true;
        if (!prefab.root_ || !visited.insert(prefab.fullPath_).second) return false;  // cycle guard
        for (XMLElement* el = prefab.root_->FirstChildElement("PrefabInstance"); el;
             el = el->NextSiblingElement("PrefabInstance")) {
            const std::string nestedPath = DirectoryOf(prefab.fullPath_) + Attr(el, "src");
            if (SamePath(nestedPath, fullPath)) return true;
            const Prefab* nested = Get(assets, nestedPath);
            if (nested && visit(*nested)) return true;
        }
        return false;
    };
    return visit(*this);
}

}  // namespace Elysium

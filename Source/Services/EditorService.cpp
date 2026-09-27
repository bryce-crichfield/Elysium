#include "Services/EditorService.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <filesystem>
#include "Components/PrefabInstanceComponent.h"
#include "Core/Xml.h"
#include "Core/Common.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Entity.h"
#include "Core/Log.h"
#include "Core/Path.h"
#include "Core/Prefab.h"
#include "Core/PrefabInstance.h"
#include "Editor/PrefabEditing.h"
#include "Interfaces/IAssetService.h"
#include "Core/Scene.h"
#include "Core/World.h"
#include "Interfaces/ISceneService.h"

namespace Elysium::Services {

EditorService::EditorService(ServiceLocator& registry) : registry_(registry) {}

void EditorService::Initialize() {
    RegisterComponentTypes();
}

void EditorService::Shutdown() {
}

Elysium::Scene* EditorService::GetInspectedScene() {
    return GetViewportScene();
}

void EditorService::RegisterComponentTypes() {
    const auto& inspectors = ComponentRegistry::Instance().GetInspectors();
    for (const auto& [name, inspectorFunc] : inspectors) {
        ComponentPlaceholder placeholder;
        placeholder.name = name;
        placeholder.drawFunc = [inspectorFunc, this](Entity e, Elysium::World* w) {
            inspectorFunc(w, e, registry_);
        };

        if (auto* access = ComponentRegistry::Instance().GetLuaAccess(name)) {
            placeholder.hasComponentFunc = [access](Entity e, Elysium::World* w) { return access->has(w, e); };
            placeholder.addComponentFunc = [access](Entity e, Elysium::World* w) { access->add(w, e); };
            placeholder.removeComponentFunc = [access](Entity e, Elysium::World* w) { access->remove(w, e); };
            placeholder.resetComponentFunc = [access](Entity e, Elysium::World* w) {
                access->remove(w, e);
                access->add(w, e);
            };
        }

        componentPlaceholders.push_back(placeholder);
    }

    // Inspector and Add Component list them in InspectorOrder, then by name.
    auto& registry = ComponentRegistry::Instance();
    std::sort(componentPlaceholders.begin(), componentPlaceholders.end(), [&](const auto& a, const auto& b) {
        const auto orderA = registry.GetInspectorOrder(a.name), orderB = registry.GetInspectorOrder(b.name);
        return orderA != orderB ? orderA < orderB : a.name < b.name;
    });
}

Elysium::World* EditorService::GetWorld() const {
    auto* doc = ActiveDocument();
    return doc ? doc->scene->GetWorld() : nullptr;
}

void EditorService::SelectEntity(Entity entity, bool additive) {
    if (!additive) {
        selectedEntities_.clear();
    }
    if (entity != INVALID_ENTITY && !IsSelected(entity)) {
        selectedEntities_.push_back(entity);
    }
}

void EditorService::ClearSelection() {
    selectedEntities_.clear();
}

bool EditorService::IsSelected(Entity entity) const {
    return std::find(selectedEntities_.begin(), selectedEntities_.end(), entity) != selectedEntities_.end();
}

void EditorService::Update(float deltaTime) {
    Profile;

    // The editor starts on the project's entry scene, as its own copy.
    auto& scenes = registry_.Get<ISceneService>();
    if (!openedEntryScene_ && !scenes.IsPlaying() && !scenes.GetEntryScene().empty()) {
        openedEntryScene_ = true;
        if (documents_.empty()) OpenScene(scenes.GetEntryScene());
    }

    // Auto-select dragged entities
    if (auto* world = GetWorld()) {
        world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
            if (bounds.isDragging) {
                SelectEntity(entity);
            }
        });
    }
}

// =============================================================================
// Documents
// =============================================================================

EditorDocument* EditorService::ActiveDocument() const {
    return activeDocument_ >= 0 && activeDocument_ < (int)documents_.size() ? documents_[activeDocument_].get() : nullptr;
}

Elysium::Scene* EditorService::GetViewportScene() {
    auto* doc = ActiveDocument();
    return doc ? doc->scene.get() : nullptr;
}

std::string EditorService::ActiveOwnerDir() const {
    auto* doc = ActiveDocument();
    return doc ? DirectoryOf(doc->fullPath) : "";
}

int EditorService::FindDocument(const std::string& fullPath) const {
    for (size_t i = 0; i < documents_.size(); ++i) {
        if (SamePath(documents_[i]->fullPath, fullPath)) return (int)i;
    }
    return -1;
}

void EditorService::AddDocument(std::unique_ptr<EditorDocument> doc) {
    LOG_INFOF("Editor", "Opened %s", doc->fullPath.c_str());
    documents_.push_back(std::move(doc));
    SetActiveDocument((int)documents_.size() - 1);
}

const Elysium::Scene* EditorService::HostScene() {
    for (const auto& doc : documents_) {
        if (!doc->IsPrefab()) return doc->scene.get();
    }
    // No scene open: borrow from the entry scene, loaded once just for its setup.
    if (!fallbackHost_) {
        auto& scenes = registry_.Get<ISceneService>();
        auto it = scenes.GetSceneRegistry().find(scenes.GetEntryScene());
        if (it == scenes.GetSceneRegistry().end()) return nullptr;
        fallbackHost_ = std::make_shared<Elysium::Scene>(registry_);
        LoadScene(*fallbackHost_, it->second.xmlPath);
    }
    return fallbackHost_.get();
}

void EditorService::OpenScene(const std::string& sceneName) {
    auto& scenes = registry_.Get<ISceneService>();
    auto it = scenes.GetSceneRegistry().find(sceneName);
    if (it == scenes.GetSceneRegistry().end() || it->second.xmlPath.empty()) {
        LOG_ERRORF("Editor", "No scene file for '%s'", sceneName.c_str());
        return;
    }
    if (int open = FindDocument(it->second.xmlPath); open >= 0) {
        SetActiveDocument(open);
        return;
    }

    auto doc = std::make_unique<EditorDocument>();
    doc->kind = EditorDocument::Kind::Scene;
    doc->fullPath = it->second.xmlPath;
    doc->title = sceneName;
    doc->scene = std::shared_ptr<Elysium::Scene>(it->second.factory(registry_));
    if (!LoadScene(*doc->scene, doc->fullPath)) return;
    AddDocument(std::move(doc));
}

void EditorService::OpenPrefab(const std::string& fullPath) {
    if (int open = FindDocument(fullPath); open >= 0) {
        SetActiveDocument(open);
        return;
    }

    const Prefab* prefab = Prefab::Get(registry_.Get<IAssetService>(), fullPath);
    if (!prefab) return;

    auto doc = std::make_unique<EditorDocument>();
    doc->kind = EditorDocument::Kind::Prefab;
    doc->fullPath = fullPath;
    doc->title = DirectoryOf(fullPath).empty() ? fullPath : fullPath.substr(DirectoryOf(fullPath).size());
    doc->parameters = prefab->GetParameters();
    doc->scene = std::make_shared<Elysium::Scene>(registry_);

    // Borrow a scene's layers and systems so the prefab renders like it would in-game.
    if (const Elysium::Scene* host = HostScene()) {
        doc->scene->CopySetupFrom(*host, false);
    } else {
        LOG_WARNING("Editor", "No scene to borrow layers/systems from; the prefab won't render.");
    }

    PrefabSpawnResult spawned = prefab->Spawn(doc->scene->GetWorld(), "", registry_);
    for (const auto& [localId, entity] : spawned.ids) doc->localIds[entity] = localId;
    AddDocument(std::move(doc));
}

void EditorService::SetActiveDocument(int index) {
    if (index < -1 || index >= (int)documents_.size()) index = -1;
    if (index != activeDocument_) ClearSelection();
    activeDocument_ = index;
    auto* doc = ActiveDocument();
    registry_.Get<ISceneService>().SetEditorScene(doc ? doc->scene.get() : nullptr);
}

void EditorService::CloseDocument(int index) {
    if (index < 0 || index >= (int)documents_.size()) return;
    // Detach from the scene service before the document's scene is destroyed.
    int active = activeDocument_;
    SetActiveDocument(-1);
    documents_.erase(documents_.begin() + index);
    // Closing the active tab moves to its neighbour, like any tabbed editor.
    if (active > index || (active == index && active >= (int)documents_.size())) active--;
    SetActiveDocument(active);
}

bool EditorService::SaveActiveDocument() {
    auto* doc = ActiveDocument();
    if (!doc) {
        LOG_ERROR("Editor", "Nothing open to save.");
        return false;
    }
    if (doc->IsPrefab()) return SavePrefabDocument(*doc);
    return SaveScene(*doc->scene, doc->fullPath);
}

// =============================================================================
// Hierarchy edits
// =============================================================================

namespace {
// Re-homes `entity` under `parent` (or makes it a root), whatever its ParentComponent said before.
void Reparent(Elysium::World& world, Entity entity, Entity parent) {
    if (world.HasComponent<ParentComponent>(entity)) {
        auto& pc = world.GetComponent<ParentComponent>(entity);
        if (pc.parent != INVALID_ENTITY) world.RemoveChild(pc.parent, entity);
        if (parent == INVALID_ENTITY) {
            world.RemoveComponent<ParentComponent>(entity);
            return;
        }
        pc.parent = INVALID_ENTITY;
        pc.targetName.clear();  // AddChild fills it with the new parent's name
    }
    if (parent != INVALID_ENTITY) world.AddChild(parent, entity);
}
}  // namespace

Entity EditorService::DuplicateSubtree(Elysium::World& world, Entity entity, Entity newParent) {
    Entity copy = INVALID_ENTITY;

    if (PrefabInstances::IsRoot(world, entity)) {
        // A new placement of the same prefab, carrying this one's overrides.
        const auto& tag = world.GetComponent<PrefabInstanceComponent>(entity);
        const std::string instanceId = tag.instanceId, ownerDir = tag.ownerDir;
        auto snapshot = PrefabEditing::Snapshot(&world, registry_, GetViewportScene(),
                                                [&](const PrefabInstanceComponent& t) { return t.instanceId == instanceId; });
        tinyxml2::XMLElement* el = snapshot->RootElement()->FirstChildElement("PrefabInstance");
        if (!el) return INVALID_ENTITY;

        std::string base = instanceId;
        while (!base.empty() && isdigit((unsigned char)base.back())) base.pop_back();
        el->SetAttribute("id", PrefabEditing::UniqueInstanceId(&world, base.empty() ? instanceId : base).c_str());

        auto spawned = PrefabInstances::Load(snapshot->RootElement(), &world, ownerDir, registry_);
        if (spawned.empty()) return INVALID_ENTITY;
        copy = PrefabInstances::RootOf(world, spawned.front());
    } else {
        copy = world.CloneEntity(entity);
    }

    Reparent(world, copy, newParent);

    // Children, except a placement's own internals (the new placement brought its own).
    const std::vector<Entity> children(world.GetChildren(entity));
    for (Entity child : children) {
        if (!PrefabInstances::IsInternal(world, child)) DuplicateSubtree(world, child, copy);
    }
    return copy;
}

Entity EditorService::PrefabRoot() const {
    auto* doc = ActiveDocument();
    if (!doc || !doc->IsPrefab()) return INVALID_ENTITY;
    auto* world = doc->scene->GetWorld();
    for (Entity entity : world->GetLivingEntities()) {
        if (world->GetParent(entity) == INVALID_ENTITY) return entity;
    }
    return INVALID_ENTITY;
}

bool EditorService::CanBeRoot(Entity entity) const {
    const Entity root = PrefabRoot();
    return root == INVALID_ENTITY || root == entity;
}

Entity EditorService::CreateEntity(Entity parent) {
    auto* world = GetWorld();
    if (!world) return INVALID_ENTITY;
    if (parent == INVALID_ENTITY && !CanBeRoot(INVALID_ENTITY)) {
        LOG_WARNING("Editor", "A prefab has a single root; create the entity under it instead.");
        return INVALID_ENTITY;
    }
    Entity entity = world->CreateEntity();
    if (parent != INVALID_ENTITY) world->AddChild(parent, entity);
    SelectEntity(entity);
    return entity;
}

Entity EditorService::DuplicateEntity(Entity entity) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY) return INVALID_ENTITY;
    entity = PrefabInstances::RootOf(*world, entity);
    if (world->GetParent(entity) == INVALID_ENTITY && !CanBeRoot(INVALID_ENTITY)) {
        LOG_WARNING("Editor", "Can't duplicate a prefab's root: a prefab has a single root.");
        return INVALID_ENTITY;
    }

    Entity copy = DuplicateSubtree(*world, entity, world->GetParent(entity));
    if (copy == INVALID_ENTITY) return INVALID_ENTITY;
    // Next to the original in the Hierarchy (roots are listed in world order).
    world->MoveEntityAfter(copy, entity);
    SelectEntity(copy);
    return copy;
}

void EditorService::DeleteEntity(Entity entity) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY) return;
    // Already gone (e.g. a descendant of an entity deleted earlier in the same batch).
    if (!world->IsAlive(entity)) return;
    entity = PrefabInstances::RootOf(*world, entity);

    // Collect the whole subtree first; destroying mutates the child lists.
    const std::vector<Entity> doomed = world->GetSubtree(entity);

    for (auto it = doomed.rbegin(); it != doomed.rend(); ++it) {
        selectedEntities_.erase(std::remove(selectedEntities_.begin(), selectedEntities_.end(), *it), selectedEntities_.end());
        world->DestroyEntity(*it);
    }
}

// Saving a prefab refreshes every placement of it (direct or nested) in the other open
// documents, so edits show up everywhere without reloading scenes. Placements
// are snapshotted before the file changes, since their overrides are diffs against it.
bool EditorService::SavePrefabDocument(EditorDocument& doc) {
    auto& assets = registry_.Get<IAssetService>();
    std::unordered_map<std::string, bool> dependsMemo;
    PrefabInstances::Filter affected = [&](const PrefabInstanceComponent& tag) {
        const std::string path = tag.FullPath();
        auto it = dependsMemo.find(path);
        if (it == dependsMemo.end()) {
            const Prefab* placed = Prefab::Get(assets, path);
            it = dependsMemo.emplace(path, placed && placed->DependsOn(doc.fullPath, assets)).first;
        }
        return it->second;
    };

    struct Pending {
        Elysium::World* world;
        std::unique_ptr<tinyxml2::XMLDocument> snapshot;
    };
    std::vector<Pending> pending;
    auto snapshot = [&](Elysium::Scene* scene) {
        if (!scene) return;
        pending.push_back({scene->GetWorld(), PrefabEditing::Snapshot(scene->GetWorld(), registry_, scene, affected)});
    };
    for (const auto& other : documents_) {
        if (other.get() != &doc) snapshot(other->scene.get());
    }

    const Elysium::Scene* host = HostScene();
    const bool ok = PrefabEditing::SaveFile(doc.scene->GetWorld(), doc.fullPath, doc.localIds, doc.parameters, registry_,
                                            host ? host : doc.scene.get());
    LOG_INFOF("Editor", "%s prefab %s", ok ? "Saved" : "Failed to save", doc.fullPath.c_str());
    if (!ok) return false;

    for (auto& p : pending) PrefabEditing::Respawn(p.world, *p.snapshot, registry_, affected);
    return true;
}

void EditorService::CreatePrefab(const std::string& directory) {
    namespace fs = std::filesystem;
    fs::path path = fs::path(directory) / "NewPrefab.xml";
    for (int n = 2; fs::exists(path); ++n) path = fs::path(directory) / ("NewPrefab" + std::to_string(n) + ".xml");

    tinyxml2::XMLDocument xml;
    xml.Parse(
        "<Prefab>\n"
        "    <Entities>\n"
        "        <Entity id=\"0\">\n"
        "            <NameComponent name=\"Root\" />\n"
        "            <TransformComponent x=\"0\" y=\"0\" />\n"
        "        </Entity>\n"
        "    </Entities>\n"
        "</Prefab>\n");
    if (!SaveXml(path.string(), xml)) return;
    Prefab::Reload(registry_.Get<IAssetService>(), path.string());  // in case a stale copy of this path is cached
    LOG_INFOF("Editor", "Created prefab %s", path.string().c_str());
    OpenPrefab(path.string());
}

bool EditorService::CreatePrefabFromEntity(Entity entity, const std::string& fullPath) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY) return false;
    if (std::filesystem::exists(fullPath)) {
        LOG_ERRORF("Editor", "Prefab already exists: %s", fullPath.c_str());
        return false;
    }
    if (!PrefabEditing::SaveSubtree(world, entity, fullPath, registry_, GetViewportScene())) {
        LOG_ERRORF("Editor", "Failed to create prefab %s", fullPath.c_str());
        return false;
    }
    LOG_INFOF("Editor", "Created prefab %s", fullPath.c_str());
    OpenPrefab(fullPath);
    return true;
}

Entity EditorService::InstantiatePrefab(const std::string& fullPath) {
    auto* world = GetWorld();
    if (!world) return INVALID_ENTITY;

    // Placing a prefab inside itself (directly or through what it places) would recurse forever.
    if (auto* doc = ActiveDocument(); doc && doc->IsPrefab()) {
        auto& assets = registry_.Get<IAssetService>();
        const Prefab* placed = Prefab::Get(assets, fullPath);
        if (placed && placed->DependsOn(doc->fullPath, assets)) {
            LOG_WARNING("Editor", "Can't place a prefab inside itself.");
            return INVALID_ENTITY;
        }
    }

    const Entity prefabRoot = PrefabRoot();
    Entity root = PrefabEditing::Instantiate(world, fullPath, ActiveOwnerDir(), registry_);
    if (root != INVALID_ENTITY && prefabRoot != INVALID_ENTITY && world->GetParent(root) == INVALID_ENTITY) {
        world->AddChild(prefabRoot, root);  // a prefab has a single root
    }
    if (root != INVALID_ENTITY && world->HasComponent<TransformComponent>(root)) {
        auto& transform = world->GetComponent<TransformComponent>(root);
        transform.localX = editorCamera_.position.x;
        transform.localY = editorCamera_.position.y;
    }
    SelectEntity(root);
    return root;
}

}  // namespace Elysium::Services

#include "Editor/EditorApplication.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <optional>
#include <filesystem>
#include <fstream>
#include "Core/Components/PrefabInstanceComponent.h"
#include "Core/Xml.h"
#include "Core/Common.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Entity.h"
#include "Core/EntitySerializer.h"
#include "Core/Log.h"
#include "Core/Path.h"
#include "Core/Prefab.h"
#include "Core/Project.h"
#include "Core/PrefabInstance.h"
#include "imgui.h"
#include "Editor/Commands/EditorCommands.h"
#include "Editor/Editor.h"
#include "Editor/EditorUI.h"
#include "Editor/Inspectors/Inspector.h"
#include "Editor/PrefabEditing.h"
#include "Editor/Style/Theme.h"
#include "Interfaces/IAssetService.h"
#include "Core/Scene.h"
#include "Core/World.h"
#include "Interfaces/ISceneService.h"

namespace Elysium {

using namespace Services;

EditorApplication::EditorApplication(ServiceLocator& services)
    : registry_(services), ui_(std::make_unique<EditorUI>(*this)) {}

EditorApplication::~EditorApplication() = default;

ServiceLocator& ServicesOf(EditorApplication& editor) { return editor.GetServices(); }

Editor::Editor(EditorApplication& editor, const std::string& name)
    : editor_(editor), services_(editor.GetServices()), name_(name) {}

void EditorApplication::Initialize(const ApplicationConfig& config) {
    RegisterComponentTypes();
    ui_->Initialize(config);
}

void EditorApplication::Draw(AppMode mode) {
    ui_->Draw(mode);
}

void EditorApplication::OnModeChanged(AppMode mode) {
    ui_->OnModeChanged(mode);
}

void EditorApplication::Shutdown() {
    ui_->Shutdown();
}

void EditorApplication::RegisterComponentTypes() {
    InspectorRegistry inspectors;
    RegisterComponentInspectors(inspectors);

    // The order each placeholder sorts by, alongside it.
    std::unordered_map<std::string, InspectorOrder> orders;
    for (const auto& [name, type] : ComponentRegistry::Instance().GetComponentTypes()) {
        const InspectorRegistry::Entry* inspector = inspectors.Find(type);
        if (!inspector) continue;
        orders[name] = inspector->order;

        ComponentPlaceholder placeholder;
        placeholder.name = name;
        placeholder.drawFunc = [draw = inspector->draw, this](Entity e, World* w) {
            draw(*w, e, *this);
        };

        if (auto* access = ComponentRegistry::Instance().GetLuaAccess(name)) {
            placeholder.hasComponentFunc = [access](Entity e, World* w) { return access->has(w, e); };
            placeholder.addComponentFunc = [access](Entity e, World* w) { access->add(w, e); };
            placeholder.removeComponentFunc = [access](Entity e, World* w) { access->remove(w, e); };
            placeholder.resetComponentFunc = [access](Entity e, World* w) {
                access->remove(w, e);
                access->add(w, e);
            };
        }

        componentPlaceholders.push_back(placeholder);
    }

    // Inspector and Add Component list them in InspectorOrder, then by name.
    std::sort(componentPlaceholders.begin(), componentPlaceholders.end(), [&](const auto& a, const auto& b) {
        const auto orderA = orders.at(a.name), orderB = orders.at(b.name);
        return orderA != orderB ? orderA < orderB : a.name < b.name;
    });
}

World* EditorApplication::GetWorld() const {
    auto* doc = ActiveDocument();
    return doc && doc->scene ? doc->scene->GetWorld() : nullptr;
}

void EditorApplication::SelectEntity(Entity entity, bool additive) {
    // A placement is opaque, so selecting anything inside one selects the placement instead. The
    // pickers already resolved this themselves, one by one; doing it here makes it true of every
    // caller, which is what keeps an instance's internals out of the Inspector for good.
    if (auto* world = GetWorld(); world && entity != INVALID_ENTITY && world->IsAlive(entity)) {
        entity = PrefabInstances::RootOf(*world, entity);
    }

    if (!additive) {
        selectedEntities_.clear();
    } else if (IsSelected(entity)) {
        // Ctrl-clicking something already selected takes it back out, which is the only way to
        // correct a multi-selection without starting it over.
        selectedEntities_.erase(std::remove(selectedEntities_.begin(), selectedEntities_.end(), entity),
                                selectedEntities_.end());
        return;
    }
    if (entity != INVALID_ENTITY && !IsSelected(entity)) {
        selectedEntities_.push_back(entity);
    }
}

void EditorApplication::ClearSelection() {
    selectedEntities_.clear();
}

bool EditorApplication::IsSelected(Entity entity) const {
    return std::find(selectedEntities_.begin(), selectedEntities_.end(), entity) != selectedEntities_.end();
}

// --- Layers and grid --------------------------------------------------------------------

namespace {
// The grid, kept in a scene's editor metadata.
void ReadGrid(const Scene& scene, GridSettings& grid) {
    const auto& metadata = scene.GetEditorMetadata();
    auto get = [&](const char* key) -> const char* {
        auto it = metadata.find(key);
        return it == metadata.end() ? nullptr : it->second.c_str();
    };
    if (const char* v = get("gridLattice")) grid.lattice = std::string(v) == "Square" ? GridLattice::Square : GridLattice::Isometric;
    if (const char* v = get("gridWidth")) grid.width = std::max(1.0f, std::strtof(v, nullptr));
    if (const char* v = get("gridHeight")) grid.height = std::max(1.0f, std::strtof(v, nullptr));
    if (const char* v = get("gridDivisor")) grid.divisor = std::max(1, std::atoi(v));
    if (const char* v = get("gridSnap")) grid.snapEnabled = std::string(v) == "true";
    if (const char* v = get("gridShow")) grid.showGrid = std::string(v) == "true";
}

void WriteGrid(Scene& scene, const GridSettings& grid) {
    auto& metadata = scene.GetEditorMetadata();
    auto number = [](float f) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%g", f);
        return std::string(buffer);
    };
    metadata["gridLattice"] = grid.lattice == GridLattice::Square ? "Square" : "Isometric";
    metadata["gridWidth"] = number(grid.width);
    metadata["gridHeight"] = number(grid.height);
    metadata["gridDivisor"] = std::to_string(grid.divisor);
    metadata["gridSnap"] = grid.snapEnabled ? "true" : "false";
    metadata["gridShow"] = grid.showGrid ? "true" : "false";
}
}  // namespace

void EditorApplication::StoreGrid(Scene& scene) { WriteGrid(scene, EditState().grid); }

EditorApplication::DocumentEditState& EditorApplication::EditState() const {
    auto* doc = ActiveDocument();
    // "" is the scratch entry used when no document is open, so every accessor stays total.
    return editState_[doc ? doc->fullPath : std::string()];
}

const std::string& EditorApplication::GetActiveLayer() const { return EditState().activeLayer; }

void EditorApplication::SetActiveLayer(const std::string& layer) { EditState().activeLayer = layer; }

LayerEditState& EditorApplication::GetLayerState(const std::string& layer) {
    auto& state = EditState();
    auto [it, inserted] = state.layers.try_emplace(layer);
    if (inserted) return it->second;
    return it->second;
}

bool EditorApplication::IsLayerHidden(const std::string& layer) const {
    auto& state = EditState();
    auto it = state.layers.find(layer);
    const bool solo = it != state.layers.end() && it->second.solo;
    // Any solo anywhere makes every non-soloed layer hidden, regardless of its own flag.
    if (state.AnySolo()) return !solo;
    return it != state.layers.end() && it->second.hidden;
}

bool EditorApplication::IsLayerLocked(const std::string& layer) const {
    auto& state = EditState();
    auto it = state.layers.find(layer);
    return it != state.layers.end() && it->second.locked;
}

std::unordered_set<std::string> EditorApplication::GetHiddenLayers() const {
    std::unordered_set<std::string> hidden;
    auto* scene = const_cast<EditorApplication*>(this)->GetViewportScene();
    if (!scene) return hidden;
    for (const auto& layer : scene->GetLayers()) {
        if (IsLayerHidden(layer.name)) hidden.insert(layer.name);
    }
    return hidden;
}

std::string EditorApplication::GetEntityLayer(Entity entity) const {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY || !world->IsAlive(entity)) return {};
    if (world->HasComponent<LayerComponent>(entity)) {
        return world->GetComponent<LayerComponent>(entity).name;
    }
    // A child of a placement (or of any entity) inherits the layer it is drawn on from the
    // nearest ancestor that declares one, which is how RenderSorter's fallback behaves too.
    for (Entity parent = world->GetParent(entity); parent != INVALID_ENTITY; parent = world->GetParent(parent)) {
        if (world->HasComponent<LayerComponent>(parent)) {
            return world->GetComponent<LayerComponent>(parent).name;
        }
    }
    return DefaultLayer();
}

// What RenderSorter draws a LayerComponent-less entity on: "default" if the scene has it,
// otherwise the bottom layer. Returning it from GetEntityLayer keeps every entity on exactly
// one layer, so hiding/locking/filtering a layer is total rather than leaking strays.
std::string EditorApplication::DefaultLayer() const {
    auto* scene = const_cast<EditorApplication*>(this)->GetViewportScene();
    if (!scene) return {};
    const auto& layers = scene->GetLayers();
    if (layers.empty()) return {};
    for (const auto& layer : layers) {
        if (layer.name == "default") return layer.name;
    }
    return layers.front().name;
}

bool EditorApplication::IsEntityLocked(Entity entity) const {
    const std::string layer = GetEntityLayer(entity);
    return !layer.empty() && IsLayerLocked(layer);
}

GridSettings& EditorApplication::GetGrid() { return EditState().grid; }

Vector2 EditorApplication::SnapToGrid(Vector2 world) const {
    const GridSettings& grid = EditState().grid;
    if (!grid.snapEnabled) return world;
    const Vector2 cell = grid.Cell();
    if (cell.x <= 0.0f || cell.y <= 0.0f) return world;

    if (grid.lattice == GridLattice::Square) {
        return {std::round(world.x / cell.x) * cell.x, std::round(world.y / cell.y) * cell.y};
    }
    // Isometric: the lattice is the diamond centres, which is a square grid rotated into
    // (x/halfW +- y/halfH). Round in that basis and come back, so snapped points land on tile
    // centres and edges rather than on an axis-aligned grid the art doesn't follow.
    // The diamond centres are the lattice generated by (halfW, halfH) and (halfW, -halfH), so a
    // centre is a*(halfW, halfH) + b*(halfW, -halfH) for integer a, b. Solving gives
    // a = (x/halfW + y/halfH)/2 and b = (x/halfW - y/halfH)/2 — the halving matters: rounding
    // the un-halved sums lands on the half-step lattice (tile centres *and* tile corners), which
    // puts two snap targets within half a cell of each other.
    const float halfW = cell.x * 0.5f, halfH = cell.y * 0.5f;
    const float a = std::round((world.x / halfW + world.y / halfH) * 0.5f);
    const float b = std::round((world.x / halfW - world.y / halfH) * 0.5f);
    return {(a + b) * halfW, (a - b) * halfH};
}

void EditorApplication::Update(float deltaTime) {
    Profile;

    // The editor starts on the project's entry scene, as its own copy.
    auto& scenes = registry_.Get<ISceneService>();
    if (!openedEntryScene_ && !scenes.IsPlaying() && !scenes.GetEntryScene().empty()) {
        openedEntryScene_ = true;
        if (documents_.empty()) OpenScene(scenes.GetEntryScene());
    }

    // The empty space around a document's world is editor chrome, so it follows the theme.
    if (auto* doc = ActiveDocument(); doc && doc->scene) {
        const ImVec4 bg = EditorStyle::CurrentPalette().ViewportBackground;
        doc->scene->SetBackgroundColor(Color{(unsigned char)(bg.x * 255), (unsigned char)(bg.y * 255), (unsigned char)(bg.z * 255), 255});
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

EditorDocument* EditorApplication::ActiveDocument() const {
    return activeDocument_ >= 0 && activeDocument_ < (int)documents_.size() ? documents_[activeDocument_].get() : nullptr;
}

Scene* EditorApplication::GetViewportScene() {
    auto* doc = ActiveDocument();
    return doc ? doc->scene.get() : nullptr;
}

std::string EditorApplication::ActiveOwnerDir() const {
    auto* doc = ActiveDocument();
    return doc ? DirectoryOf(doc->fullPath) : "";
}

int EditorApplication::FindDocument(const std::string& fullPath) const {
    for (size_t i = 0; i < documents_.size(); ++i) {
        if (SamePath(documents_[i]->fullPath, fullPath)) return (int)i;
    }
    return -1;
}

void EditorApplication::AddDocument(std::unique_ptr<EditorDocument> doc) {
    LOG_INFOF("Editor", "Opened %s", doc->fullPath.c_str());
    documents_.push_back(std::move(doc));

    // Parallel entry, so bindings_ and documents_ stay index-aligned even for the documents
    // (content panes) that have no world to listen to.
    auto binding = std::make_unique<StableIdBinding>();
    binding->document = documents_.back().get();
    if (auto* world = documents_.back()->scene ? documents_.back()->scene->GetWorld() : nullptr) {
        world->AddWorldListener(binding.get());
    }
    stableIdBindings_.push_back(std::move(binding));

    SetActiveDocument((int)documents_.size() - 1);
}

const Scene* EditorApplication::HostScene() {
    for (const auto& doc : documents_) {
        if (doc->IsScene()) return doc->scene.get();
    }
    // No scene open: borrow from the entry scene, loaded once just for its setup.
    if (!fallbackHost_) {
        auto& scenes = registry_.Get<ISceneService>();
        if (scenes.GetEntryScene().empty()) return nullptr;
        fallbackHost_ = std::make_shared<Scene>(registry_);
        LoadScene(*fallbackHost_, ScenePath(scenes.GetEntryScene()));
    }
    return fallbackHost_.get();
}

void EditorApplication::OpenScene(const std::string& sceneName) { OpenSceneFile(ScenePath(sceneName)); }

void EditorApplication::OpenSceneFile(const std::string& fullPath) {
    std::error_code ec;
    if (!std::filesystem::exists(fullPath, ec)) {
        LOG_ERRORF("Editor", "No scene file %s", fullPath.c_str());
        return;
    }
    if (int open = FindDocument(fullPath); open >= 0) {
        SetActiveDocument(open);
        return;
    }

    auto doc = std::make_unique<EditorDocument>();
    doc->kind = AssetKind::Scene;
    doc->fullPath = fullPath;
    doc->scene = std::make_shared<Scene>(registry_);
    if (!LoadScene(*doc->scene, doc->fullPath)) return;
    doc->title = doc->scene->GetName();
    ReadGrid(*doc->scene, editState_[doc->fullPath].grid);
    AddDocument(std::move(doc));
}

bool EditorApplication::RenameScene(Scene& scene, const std::string& newName) {
    auto it = std::find_if(documents_.begin(), documents_.end(), [&](const auto& d) { return d->scene.get() == &scene; });
    if (it == documents_.end() || !(*it)->IsScene()) return false;
    EditorDocument& doc = **it;
    const std::string oldName = scene.GetName();
    if (newName == oldName) return true;
    if (newName.empty() || newName.find_first_of("/\\:*?\"<>|.") != std::string::npos) {
        LOG_ERRORF("Editor", "'%s' isn't a valid scene name", newName.c_str());
        return false;
    }
    const std::string newPath = DirectoryOf(doc.fullPath) + newName + ".xml";
    std::error_code ec;
    if (std::filesystem::exists(newPath, ec)) {
        LOG_ERRORF("Editor", "A scene called '%s' already exists", newName.c_str());
        return false;
    }
    std::filesystem::rename(doc.fullPath, newPath, ec);
    if (ec) {
        LOG_ERRORF("Editor", "Couldn't rename %s: %s", doc.fullPath.c_str(), ec.message().c_str());
        return false;
    }

    // The tab's grid and layer state are keyed by its path.
    if (auto state = editState_.extract(doc.fullPath)) {
        state.key() = newPath;
        editState_.insert(std::move(state));
    }
    doc.fullPath = newPath;
    doc.title = newName;
    scene.SetSource(newName, newPath);

    auto& scenes = registry_.Get<ISceneService>();
    if (scenes.GetEntryScene() == oldName) {
        scenes.SetEntryScene(newName);
        const std::string projectFile = Path::GetAssetsRoot() + "Project.xml";
        if (!ProjectConfig::SetEntryScene(projectFile, newName)) {
            LOG_WARNINGF("Editor", "Couldn't update the entry scene in %s", projectFile.c_str());
        }
    }
    LOG_INFOF("Editor", "Renamed scene '%s' to '%s'", oldName.c_str(), newName.c_str());
    return true;
}

void EditorApplication::OpenAsset(const std::string& fullPath) {
    const std::optional<AssetKind> kind = AssetKindOf(Path::FromFullPath(fullPath).GetRelativePath());
    if (!kind || *kind == AssetKind::Folder || *kind == AssetKind::Font) return;
    if (*kind == AssetKind::Prefab) return OpenPrefab(fullPath);
    if (*kind == AssetKind::Scene) {
        return OpenSceneFile(fullPath);
    }

    if (int open = FindDocument(fullPath); open >= 0) {
        SetActiveDocument(open);
        return;
    }
    auto doc = std::make_unique<EditorDocument>();
    doc->kind = *kind;
    doc->fullPath = fullPath;
    doc->title = std::filesystem::path(fullPath).filename().string();
    AddDocument(std::move(doc));
}

void EditorApplication::OpenPrefab(const std::string& fullPath) {
    if (int open = FindDocument(fullPath); open >= 0) {
        SetActiveDocument(open);
        return;
    }

    const Prefab* prefab = Prefab::Get(registry_.Get<IAssetService>(), fullPath);
    if (!prefab) return;

    auto doc = std::make_unique<EditorDocument>();
    doc->kind = AssetKind::Prefab;
    doc->fullPath = fullPath;
    doc->title = DirectoryOf(fullPath).empty() ? fullPath : fullPath.substr(DirectoryOf(fullPath).size());
    doc->parameters = prefab->GetParameters();
    doc->scene = std::make_shared<Scene>(registry_);

    // Borrow a scene's layers and systems, so the prefab's entities find their layers and are
    // drawn in the right space. But a preview, not the scene's look: each layer is drawn plain,
    // without its lighting, fog, compositing or blending.
    if (const Scene* host = HostScene()) {
        doc->scene->CopySetupFrom(*host, false);
        for (SceneLayer& layer : doc->scene->GetLayers()) {
            layer.isVisible = true;
            layer.isComposited = false;
            layer.opacity = 1.0f;
            layer.layerBlend = layer.compositeBlend = SceneLayerBlend::Normal;
            layer.ambient = {0, 0, 0, 0};
            layer.fogOfWar = 0.0f;
            layer.lightAmbient = {220, 220, 224, 255};  // World3D: an even light, so models read
        }
    } else {
        LOG_WARNING("Editor", "No scene to borrow layers/systems from; the prefab won't render.");
    }

    PrefabSpawnResult spawned = prefab->Spawn(doc->scene->GetWorld(), "", registry_);
    for (const auto& [localId, entity] : spawned.ids) doc->localIds[entity] = localId;
    AddDocument(std::move(doc));
}

void EditorApplication::SetActiveDocument(int index) {
    if (index < -1 || index >= (int)documents_.size()) index = -1;
    if (index != activeDocument_) ClearSelection();
    activeDocument_ = index;
    auto* doc = ActiveDocument();
    registry_.Get<ISceneService>().SetEditorScene(doc ? doc->scene.get() : nullptr);
}

void EditorApplication::CloseDocument(int index) {
    if (index < 0 || index >= (int)documents_.size()) return;
    // Detach from the scene service before the document's scene is destroyed.
    int active = activeDocument_;
    SetActiveDocument(-1);
    if (index < (int)stableIdBindings_.size()) {
        if (auto* world = documents_[index]->scene ? documents_[index]->scene->GetWorld() : nullptr) {
            world->RemoveWorldListener(stableIdBindings_[index].get());
        }
        stableIdBindings_.erase(stableIdBindings_.begin() + index);
    }
    documents_.erase(documents_.begin() + index);
    // Closing the active tab moves to its neighbour, like any tabbed editor.
    if (active > index || (active == index && active >= (int)documents_.size())) active--;
    SetActiveDocument(active);
}

bool EditorApplication::SaveActiveDocument() {
    auto* doc = ActiveDocument();
    if (!doc) {
        LOG_ERROR("Editor", "Nothing open to save.");
        return false;
    }
    const bool saved = doc->IsPrefab()  ? SavePrefabDocument(*doc)
                       : doc->IsScene() ? (StoreGrid(*doc->scene), SaveScene(*doc->scene, doc->fullPath))
                                        : false;
    if (saved) doc->history.MarkSaved();
    return saved;
}

// =============================================================================
// Commands
// =============================================================================

std::optional<CommandContext> EditorApplication::CommandCtx() {
    auto* world = GetWorld();
    if (!world) return std::nullopt;
    return CommandContext{*world, *this, registry_};
}

CommandHistory* EditorApplication::GetHistory() {
    auto* doc = ActiveDocument();
    return doc && doc->scene ? &doc->history : nullptr;
}

void EditorApplication::Execute(std::unique_ptr<EditorCommand> command) {
    auto* history = GetHistory();
    auto context = CommandCtx();
    if (!history || !context || !command) return;
    history->Execute(*context, std::move(command));
}

void EditorApplication::Undo() {
    auto* history = GetHistory();
    auto context = CommandCtx();
    if (!history || !context || !history->CanUndo()) return;
    history->Undo(*context);
    PruneSelection();
}

void EditorApplication::Redo() {
    auto* history = GetHistory();
    auto context = CommandCtx();
    if (!history || !context || !history->CanRedo()) return;
    history->Redo(*context);
    PruneSelection();
}

// The grouping calls are no-ops without a document rather than errors, so a caller can wrap a
// batch unconditionally instead of guarding every one.
void EditorApplication::BeginTransaction(const std::string& label) {
    if (auto* history = GetHistory()) history->BeginTransaction(label);
}

void EditorApplication::EndTransaction() {
    if (auto* history = GetHistory()) history->EndTransaction();
}

void EditorApplication::BeginGesture(const std::string& label) {
    if (auto* history = GetHistory()) history->BeginGesture(label);
}

void EditorApplication::EndGesture() {
    if (auto* history = GetHistory()) history->EndGesture();
}

void EditorApplication::PruneSelection() {
    auto* world = GetWorld();
    if (!world) return;
    selectedEntities_.erase(
        std::remove_if(selectedEntities_.begin(), selectedEntities_.end(),
                       [world](Entity e) { return !world->IsAlive(e); }),
        selectedEntities_.end());
}

// --- Stable entity references -------------------------------------------------------------

uint64_t EditorApplication::StableIdOf(Entity entity) {
    auto* doc = ActiveDocument();
    if (!doc || entity == INVALID_ENTITY) return 0;

    auto existing = doc->stableIdByEntity.find(entity);
    if (existing != doc->stableIdByEntity.end()) return existing->second;

    const uint64_t id = doc->nextStableId++;
    doc->stableIdByEntity[entity] = id;
    doc->entityByStableId[id] = entity;
    return id;
}

Entity EditorApplication::EntityForStableId(uint64_t id) const {
    auto* doc = ActiveDocument();
    if (!doc || id == 0) return INVALID_ENTITY;
    auto it = doc->entityByStableId.find(id);
    return it == doc->entityByStableId.end() ? INVALID_ENTITY : it->second;
}

void EditorApplication::RebindStableId(uint64_t id, Entity entity) {
    auto* doc = ActiveDocument();
    if (!doc || id == 0) return;

    // Drop whatever this id pointed at, and any id that entity already had, so the two maps
    // stay exact inverses of each other.
    auto previous = doc->entityByStableId.find(id);
    if (previous != doc->entityByStableId.end()) doc->stableIdByEntity.erase(previous->second);

    if (entity == INVALID_ENTITY) {
        doc->entityByStableId.erase(id);
        return;
    }

    auto stale = doc->stableIdByEntity.find(entity);
    if (stale != doc->stableIdByEntity.end()) doc->entityByStableId.erase(stale->second);

    doc->entityByStableId[id] = entity;
    doc->stableIdByEntity[entity] = id;
}

void EditorApplication::StableIdBinding::OnEntityDestroyed(Entity entity) {
    if (!document) return;
    auto it = document->stableIdByEntity.find(entity);
    if (it == document->stableIdByEntity.end()) return;
    document->entityByStableId.erase(it->second);
    document->stableIdByEntity.erase(it);
}


// =============================================================================
// Clipboard
// =============================================================================

namespace {
// The clipboard document: one <Subtree> per copied root, so a multi-selection pastes as a
// group rather than as unrelated fragments.
constexpr const char* kClipboardRoot = "ElysiumClipboard";

// The roots of `selection`. An entity whose ancestor is also selected already travels inside
// that ancestor's subtree, so copying it separately would duplicate it on paste.
std::vector<Entity> TopLevelOf(World& world, const std::vector<Entity>& selection) {
    std::vector<Entity> roots;
    for (Entity entity : selection) {
        if (!world.IsAlive(entity)) continue;
        bool covered = false;
        for (Entity other : selection) {
            if (other != entity && world.IsAncestorOf(other, entity)) { covered = true; break; }
        }
        if (!covered) roots.push_back(entity);
    }
    return roots;
}

Vector2 PositionOf(World& world, Entity entity) {
    if (!world.HasComponent<TransformComponent>(entity)) return {0.0f, 0.0f};
    const auto& t = world.GetComponent<TransformComponent>(entity);
    return { t.worldX, t.worldY };
}
}  // namespace

void EditorApplication::CopySelection() {
    auto* world = GetWorld();
    if (!world || selectedEntities_.empty()) return;

    const std::vector<Entity> roots = TopLevelOf(*world, selectedEntities_);
    if (roots.empty()) return;

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* clipboard = doc.NewElement(kClipboardRoot);
    doc.InsertFirstChild(clipboard);

    // Each root's offset from the first one, so a paste keeps the arrangement instead of
    // stacking everything on a single spot.
    const Vector2 origin = PositionOf(*world, roots.front());

    for (Entity root : roots) {
        const std::string xml = EntityXml::SaveSubtree(*world, root);
        if (xml.empty()) continue;

        tinyxml2::XMLDocument parsed;
        if (parsed.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS || !parsed.RootElement()) continue;

        tinyxml2::XMLNode* copied = parsed.RootElement()->DeepClone(&doc);
        if (!copied) continue;
        const Vector2 position = PositionOf(*world, root);
        copied->ToElement()->SetAttribute("offsetX", position.x - origin.x);
        copied->ToElement()->SetAttribute("offsetY", position.y - origin.y);
        clipboard->InsertEndChild(copied);
    }

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    ImGui::SetClipboardText(printer.CStr());
}

void EditorApplication::CutSelection() {
    auto* world = GetWorld();
    if (!world || selectedEntities_.empty()) return;

    CopySelection();
    const std::vector<Entity> roots = TopLevelOf(*world, selectedEntities_);

    BeginTransaction("Cut Entities");
    for (Entity root : roots) DeleteEntity(root);
    EndTransaction();
}

bool EditorApplication::CanPaste() const {
    const char* text = ImGui::GetClipboardText();
    return text && std::string(text).find(kClipboardRoot) != std::string::npos;
}

Entity EditorApplication::Paste(Vector2 at) {
    auto* world = GetWorld();
    const char* text = ImGui::GetClipboardText();
    if (!world || !text) return INVALID_ENTITY;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(text) != tinyxml2::XML_SUCCESS) return INVALID_ENTITY;
    tinyxml2::XMLElement* clipboard = doc.RootElement();
    if (!clipboard || std::string(clipboard->Name()) != kClipboardRoot) return INVALID_ENTITY;

    const Vector2 anchor = SnapToGrid(at);
    const std::string layer = GetActiveLayer();
    Entity first = INVALID_ENTITY;
    std::vector<Entity> pasted;

    BeginTransaction("Paste");
    for (tinyxml2::XMLElement* subtree = clipboard->FirstChildElement(); subtree;
         subtree = subtree->NextSiblingElement()) {
        if (!CanBeRoot(INVALID_ENTITY)) {
            LOG_WARNING("Editor", "A prefab has a single root; paste into a scene instead.");
            break;
        }

        tinyxml2::XMLPrinter printer;
        subtree->Accept(&printer);

        std::vector<Entity> created;
        const Entity root = EntityXml::LoadSubtree(*world, printer.CStr(), INVALID_ENTITY, registry_, &created);
        if (root == INVALID_ENTITY) continue;

        // A pasted placement is a new placement, so it needs an instance id of its own -- sharing
        // the original's would merge the two into one <PrefabInstance> the next time this saves.
        PrefabInstances::Reinstance(world, created);

        if (world->HasComponent<TransformComponent>(root)) {
            auto& transform = world->GetComponent<TransformComponent>(root);
            transform.localX = anchor.x + subtree->FloatAttribute("offsetX");
            transform.localY = anchor.y + subtree->FloatAttribute("offsetY");
            transform.worldX = transform.localX;
            transform.worldY = transform.localY;
        }
        // Pasted entities join the layer being worked on rather than the one they were copied
        // from: the active layer is where the user is looking.
        if (!layer.empty()) {
            if (world->HasComponent<LayerComponent>(root)) {
                world->GetComponent<LayerComponent>(root).name = layer;
            } else {
                world->AddComponent<LayerComponent>(root, LayerComponent(layer));
            }
        }

        RecordSpawn(root, "Paste");
        pasted.push_back(root);
        if (first == INVALID_ENTITY) first = root;
    }
    EndTransaction();

    // Select what landed, so it can be dragged straight into place.
    ClearSelection();
    for (Entity entity : pasted) SelectEntity(entity, true);
    return first;
}


// =============================================================================
// Hierarchy edits
// =============================================================================

Entity EditorApplication::DuplicateSubtree(World& world, Entity entity, Entity newParent) {
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

    EditorOps::Reparent(world, copy, newParent);

    // Children, except a placement's own internals (the new placement brought its own).
    const std::vector<Entity> children(world.GetChildren(entity));
    for (Entity child : children) {
        if (!PrefabInstances::IsInternal(world, child)) DuplicateSubtree(world, child, copy);
    }
    return copy;
}

Entity EditorApplication::PrefabRoot() const {
    auto* doc = ActiveDocument();
    if (!doc || !doc->IsPrefab()) return INVALID_ENTITY;
    auto* world = doc->scene->GetWorld();
    for (Entity entity : world->GetLivingEntities()) {
        if (world->GetParent(entity) == INVALID_ENTITY) return entity;
    }
    return INVALID_ENTITY;
}

bool EditorApplication::CanBeRoot(Entity entity) const {
    const Entity root = PrefabRoot();
    return root == INVALID_ENTITY || root == entity;
}

Entity EditorApplication::CreateEntity(Entity parent) {
    auto* world = GetWorld();
    if (!world) return INVALID_ENTITY;
    if (parent == INVALID_ENTITY && !CanBeRoot(INVALID_ENTITY)) {
        LOG_WARNING("Editor", "A prefab has a single root; create the entity under it instead.");
        return INVALID_ENTITY;
    }
    Entity entity = world->CreateEntity();
    if (parent != INVALID_ENTITY) {
        world->AddChild(parent, entity);
    } else if (const std::string& layer = GetActiveLayer(); !layer.empty()) {
        // Root entities land on the layer being worked on. The layer drawer's focus is what
        // the whole placement workflow is organised around, and an entity with no
        // LayerComponent silently falls back to the default layer, which is rarely the one
        // that was meant. Children inherit through GetEntityLayer, so they need none.
        world->AddComponent<LayerComponent>(entity, LayerComponent(layer));
    }
    SelectEntity(entity);
    RecordSpawn(entity, "Create Entity");
    return entity;
}

void EditorApplication::RecordSpawn(Entity entity, const std::string& label) {
    if (entity == INVALID_ENTITY) return;
    auto context = CommandCtx();
    if (!context) return;
    Execute(std::make_unique<SpawnCommand>(*context, entity, label));
}

void EditorApplication::Reparent(Entity entity, Entity parent) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY || entity == parent) return;
    if (!world->IsAlive(entity)) return;
    // Reparenting an entity under its own descendant would detach the whole branch from the
    // world; World::AddChild has no cycle check of its own.
    if (parent != INVALID_ENTITY && world->IsAncestorOf(entity, parent)) return;
    if (parent == INVALID_ENTITY && !CanBeRoot(entity)) {
        LOG_WARNING("Editor", "A prefab has a single root; keep the entity under it.");
        return;
    }
    if (world->GetParent(entity) == parent) return;

    auto context = CommandCtx();
    if (!context) return;
    Execute(std::make_unique<ReparentCommand>(*context, entity, parent));
}

void EditorApplication::ReorderBefore(Entity entity, Entity sibling) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY || sibling == INVALID_ENTITY || entity == sibling) return;
    auto context = CommandCtx();
    if (!context) return;
    Execute(std::make_unique<ReorderCommand>(*context, entity, sibling, true));
}

void EditorApplication::ReorderAfter(Entity entity, Entity sibling) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY || sibling == INVALID_ENTITY || entity == sibling) return;
    auto context = CommandCtx();
    if (!context) return;
    Execute(std::make_unique<ReorderCommand>(*context, entity, sibling, false));
}

Entity EditorApplication::DuplicateEntity(Entity entity) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY) return INVALID_ENTITY;
    entity = PrefabInstances::RootOf(*world, entity);
    if (IsEntityLocked(entity)) {
        LOG_WARNINGF("Editor", "Layer '%s' is locked.", GetEntityLayer(entity).c_str());
        return INVALID_ENTITY;
    }
    if (world->GetParent(entity) == INVALID_ENTITY && !CanBeRoot(INVALID_ENTITY)) {
        LOG_WARNING("Editor", "Can't duplicate a prefab's root: a prefab has a single root.");
        return INVALID_ENTITY;
    }

    Entity copy = DuplicateSubtree(*world, entity, world->GetParent(entity));
    if (copy == INVALID_ENTITY) return INVALID_ENTITY;
    // Next to the original in the Hierarchy (roots are listed in world order).
    world->MoveEntityAfter(copy, entity);
    SelectEntity(copy);
    RecordSpawn(copy, "Duplicate Entity");
    return copy;
}

void EditorApplication::DeleteEntity(Entity entity) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY) return;
    // Already gone (e.g. a descendant of an entity deleted earlier in the same batch).
    if (!world->IsAlive(entity)) return;
    entity = PrefabInstances::RootOf(*world, entity);
    if (IsEntityLocked(entity)) {
        LOG_WARNINGF("Editor", "Layer '%s' is locked.", GetEntityLayer(entity).c_str());
        return;
    }

    // Deselect here rather than inside the command: the selection is editor session state,
    // not document state, so undoing the delete should bring the entity back without also
    // re-selecting it.
    for (Entity doomed : world->GetSubtree(entity)) {
        selectedEntities_.erase(std::remove(selectedEntities_.begin(), selectedEntities_.end(), doomed),
                                selectedEntities_.end());
    }

    auto context = CommandCtx();
    if (!context) return;
    Execute(std::make_unique<DeleteEntityCommand>(*context, entity));
}

// Saving a prefab refreshes every placement of it (direct or nested) in the other open
// documents, so edits show up everywhere without reloading scenes. Placements
// are snapshotted before the file changes, since their overrides are diffs against it.
bool EditorApplication::SavePrefabDocument(EditorDocument& doc) {
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
        World* world;
        std::unique_ptr<tinyxml2::XMLDocument> snapshot;
    };
    std::vector<Pending> pending;
    auto snapshot = [&](Scene* scene) {
        if (!scene) return;
        pending.push_back({scene->GetWorld(), PrefabEditing::Snapshot(scene->GetWorld(), registry_, scene, affected)});
    };
    for (const auto& other : documents_) {
        if (other.get() != &doc) snapshot(other->scene.get());
    }

    const Scene* host = HostScene();
    const bool ok = PrefabEditing::SaveFile(doc.scene->GetWorld(), doc.fullPath, doc.localIds, doc.parameters, registry_,
                                            host ? host : doc.scene.get());
    LOG_INFOF("Editor", "%s prefab %s", ok ? "Saved" : "Failed to save", doc.fullPath.c_str());
    if (!ok) return false;

    for (auto& p : pending) PrefabEditing::Respawn(p.world, *p.snapshot, registry_, affected);
    return true;
}

namespace {
// What a new file of each kind starts as. Scenes aren't here: they copy a scene's setup.
const char* StarterText(AssetKind kind, const std::string& name) {
    static std::string text;
    switch (kind) {
        case AssetKind::Prefab:
            text = "<Prefab>\n"
                   "    <Entities>\n"
                   "        <Entity id=\"0\">\n"
                   "            <NameComponent name=\"" + name + "\" />\n"
                   "            <TransformComponent x=\"0\" y=\"0\" />\n"
                   "        </Entity>\n"
                   "    </Entities>\n"
                   "</Prefab>\n";
            break;
        case AssetKind::Script:
            text = "---@type EntityScript\n"
                   "local " + name + " = {}\n\n"
                   "function " + name + ":Initialize(entity)\n"
                   "end\n\n"
                   "function " + name + ":Update(entity, dt)\n"
                   "end\n\n"
                   "return " + name + "\n";
            break;
        case AssetKind::Sprite:
            text = "<Sprite name=\"" + name + "\" originX=\"0.5\" originY=\"0.5\">\n"
                   "    <Sequences>\n"
                   "        <Sequence name=\"default\" indicies=\"0:*\" />\n"
                   "    </Sequences>\n"
                   "</Sprite>\n";
            break;
        case AssetKind::Shader:
            text = "#version 330\n\n"
                   "in vec2 fragTexCoord;\n"
                   "in vec4 fragColor;\n\n"
                   "uniform sampler2D texture0;\n"
                   "uniform vec4 colDiffuse;\n\n"
                   "out vec4 finalColor;\n\n"
                   "void main()\n"
                   "{\n"
                   "    finalColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;\n"
                   "}\n";
            break;
        default: return nullptr;
    }
    return text.c_str();
}
}  // namespace

bool EditorApplication::CreateAsset(AssetKind kind, const std::string& fullPath) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(fullPath, ec)) {
        LOG_ERRORF("Editor", "%s already exists", fullPath.c_str());
        return false;
    }
    fs::create_directories(fs::path(fullPath).parent_path(), ec);

    bool ok = false;
    if (kind == AssetKind::Scene) {
        // An empty scene with the layers and systems of the scene prefabs borrow from.
        Scene scene(registry_);
        if (const Scene* host = HostScene()) scene.CopySetupFrom(*host, false);
        ok = SaveScene(scene, fullPath);
    } else if (const char* text = StarterText(kind, fs::path(fullPath).stem().string())) {
        std::ofstream file(fullPath, std::ios::binary);
        file << text;
        ok = file.good();
    }
    LOG_INFOF("Editor", "%s %s", ok ? "Created" : "Failed to create", fullPath.c_str());
    if (!ok) return false;
    if (kind == AssetKind::Prefab) Prefab::Reload(registry_.Get<IAssetService>(), fullPath);  // in case a stale copy is cached
    OpenAsset(fullPath);
    return true;
}

bool EditorApplication::SaveActiveDocumentAs(const std::string& fullPath) {
    auto* doc = ActiveDocument();
    if (!doc || !doc->HasWorld()) return false;
    bool ok = false;
    if (doc->IsScene()) {
        StoreGrid(*doc->scene);
        ok = SaveScene(*doc->scene, fullPath);
    } else {
        std::unordered_map<Entity, int> localIds = doc->localIds;  // the original keeps its own
        const Scene* host = HostScene();
        ok = PrefabEditing::SaveFile(doc->scene->GetWorld(), fullPath, localIds, doc->parameters, registry_,
                                     host ? host : doc->scene.get());
    }
    LOG_INFOF("Editor", "%s %s", ok ? "Saved" : "Failed to save", fullPath.c_str());
    if (ok) ReplaceActiveDocument(fullPath);
    return ok;
}

void EditorApplication::ReplaceActiveDocument(const std::string& fullPath) {
    const int old = activeDocument_;
    OpenAsset(fullPath);
    // The new tab opened at the end; take the old one's place.
    if (old >= 0 && activeDocument_ != old && old < (int)documents_.size()) {
        auto moved = std::move(documents_[activeDocument_]);
        documents_.erase(documents_.begin() + activeDocument_);
        documents_.insert(documents_.begin() + old, std::move(moved));
        activeDocument_ = old;
        CloseDocument(old + 1);
        SetActiveDocument(old);
    }
}

bool EditorApplication::CreatePrefabFromEntity(Entity entity, const std::string& fullPath) {
    auto* world = GetWorld();
    if (!world || entity == INVALID_ENTITY) return false;
    if (std::filesystem::exists(fullPath)) {
        LOG_ERRORF("Editor", "Prefab already exists: %s", fullPath.c_str());
        return false;
    }
    if (entity == PrefabRoot()) {
        LOG_WARNING("Editor", "A prefab's root is the prefab itself; pack entities under it instead.");
        return false;
    }
    if (!PrefabEditing::SaveSubtree(world, entity, fullPath, registry_, GetViewportScene())) {
        LOG_ERRORF("Editor", "Failed to create prefab %s", fullPath.c_str());
        return false;
    }
    LOG_INFOF("Editor", "Packed prefab %s", fullPath.c_str());

    // The subtree is replaced by a placement of the new prefab, where it was.
    const Entity parent = world->GetParent(entity);
    std::optional<TransformComponent> transform;
    if (world->HasComponent<TransformComponent>(entity)) transform = world->GetComponent<TransformComponent>(entity);
    DeleteEntity(entity);
    const Entity root = PrefabEditing::Instantiate(world, fullPath, ActiveOwnerDir(), registry_);
    if (root == INVALID_ENTITY) return false;
    if (parent != INVALID_ENTITY) world->AddChild(parent, root);
    if (transform && world->HasComponent<TransformComponent>(root)) {
        auto& placed = world->GetComponent<TransformComponent>(root);
        placed.localX = transform->localX;
        placed.localY = transform->localY;
        placed.localScaleX = transform->localScaleX;
        placed.localScaleY = transform->localScaleY;
        placed.localRotation = transform->localRotation;
    }
    SelectEntity(root);
    return true;
}

Entity EditorApplication::InstantiatePrefab(const std::string& fullPath) {
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
    RecordSpawn(root, "Place Prefab");
    return root;
}

}  // namespace Elysium

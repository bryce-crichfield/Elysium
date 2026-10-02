#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Core/Application.h"
#include "Core/AssetKind.h"
#include "Core/Entity.h"
#include "Core/Event.h"
#include "Core/Math/MathTypes.h"
#include "Core/Prefab.h"
#include "Core/ServiceLocator.h"
#include "Core/World.h"  // IWorldListener; needs Entity.h and Event.h ahead of it
#include "Editor/Commands/CommandHistory.h"
#include "Editor/Commands/EditorCommand.h"

namespace Elysium {

class Scene;
class EditorUI;

struct ComponentPlaceholder {
    std::function<void(Entity, World*)> drawFunc;
    std::function<bool(Entity, World*)> hasComponentFunc;
    std::function<void(Entity, World*)> addComponentFunc;
    std::function<void(Entity, World*)> removeComponentFunc;
    std::function<void(Entity, World*)> resetComponentFunc;
    std::string name;
};

// The editor's free/independent camera — decoupled from any in-scene CameraComponent so
// the viewport can be panned/zoomed around the scene without touching game state.
struct EditorCamera {
    Vector2 position = {0, 0};
    float zoom = 1.0f;
    // Orbit about `position` (degrees; see World3D::View). The target is where the orientation
    // widget's snaps ease toward; dragging sets both.
    float yaw = 0.0f, pitch = 30.0f;
    float targetYaw = 0.0f, targetPitch = 30.0f;
    bool initialized = false;
};

// An asset open for editing in its own viewport tab. Scenes and prefabs have a world: their
// entities live in a private Scene loaded from disk, never on the game's stack, so editing
// and playing never share state (a prefab borrows a scene's layers and systems so it
// renders the same way). Every other kind is edited by its content pane from the file.
struct EditorDocument {
    AssetKind kind = AssetKind::Prefab;
    std::string fullPath;
    std::string title;
    std::shared_ptr<Scene> scene;  // scenes and prefabs only
    // Prefab only:
    std::unordered_map<Entity, int> localIds;     // entity -> <Entity id> in the file
    std::vector<PrefabParameter> parameters;

    // Undo/redo for this tab, so each open scene has its own history and closing the tab
    // discards it. Scenes and prefabs only; a content pane has nothing to undo through here.
    CommandHistory history;

    // Stable entity references for the history. `Entity` is a recycled index with no
    // generation counter (Core/Entity.h), so a command holding one could address a different
    // entity entirely after an undo destroyed and recreated something. Commands hold a stable
    // id instead and resolve it through here; undo rebinds the same id to what it recreates.
    std::unordered_map<uint64_t, Entity> entityByStableId;
    std::unordered_map<Entity, uint64_t> stableIdByEntity;
    uint64_t nextStableId = 1;

    bool IsPrefab() const { return kind == AssetKind::Prefab; }
    bool IsScene() const { return kind == AssetKind::Scene; }
    // Scenes and prefabs: the Hierarchy and Inspector work on this document's entities.
    bool HasWorld() const { return scene != nullptr; }
};

// Per-layer editing state, for the Viewport's layer drawer. All of it is editor session state:
// it is keyed by document and never written to the scene file, so soloing a layer to work on it
// can't be saved into the scene by accident.
struct LayerEditState {
    bool locked = false;  // entities on it can't be picked, dragged or deleted
    bool solo = false;    // while any layer is soloed, only soloed layers draw
    bool hidden = false;
};

enum class GridLattice { Square, Isometric };

// The editing grid: what snapping lands on and what the viewport draws. Session state too —
// edited on the Scene Settings screen but deliberately not part of SceneConfiguration, so it
// never reaches the scene file.
struct GridSettings {
    bool snapEnabled = false;
    bool showGrid = false;
    GridLattice lattice = GridLattice::Isometric;
    // One cell, in world units. Isometric treats these as the full diamond width/height.
    float width = 64.0f;
    float height = 32.0f;
    // Snap to cell / divisor, so half- and quarter-cell placement doesn't need a resize.
    int divisor = 1;

    Vector2 Cell() const { return {width / (float)(divisor > 0 ? divisor : 1), height / (float)(divisor > 0 ? divisor : 1)}; }
};

// The in-engine editor. Application owns one and drives it: Initialize once the engine's
// services are up, Update and Draw every frame, OnModeChanged on F1/F2, Shutdown last. It is
// not an engine service: nothing outside Editor/ sees it, and the editor's panels, tools and
// commands are handed it directly.
//
// It holds the editing session (the open documents, each scene or prefab its own Scene and
// never the game's; selection; undo; clipboard; layer and grid state; the editor camera) and
// owns its UI, EditorUI (ImGui, the panels, menus and dock layout).
class EditorApplication {
   public:
    explicit EditorApplication(ServiceLocator& services);
    ~EditorApplication();

    void Initialize(const ApplicationConfig& config);
    void Update(float deltaTime);
    void Draw(AppMode mode);
    void OnModeChanged(AppMode mode);
    void Shutdown();

    // The engine's services, for editor code that needs the scenes, assets or scripts.
    ServiceLocator& GetServices() { return registry_; }

    World* GetWorld() const;

    // --- Layers (active document) ------------------------------------------------------
    // The layer being worked on: it filters the Hierarchy and receives painted prefabs.
    // Empty means no layer is focused, and the Hierarchy shows everything.
    const std::string& GetActiveLayer() const;
    void SetActiveLayer(const std::string& layer);

    LayerEditState& GetLayerState(const std::string& layer);

    // Effective visibility and lock, with solo resolved: once any layer is soloed, every layer
    // that isn't soloed counts as hidden.
    bool IsLayerHidden(const std::string& layer) const;
    bool IsLayerLocked(const std::string& layer) const;
    // Every layer that should not draw this frame. RenderSorter applies this to its own copy
    // of the layer list, so SceneLayer::isVisible in the scene is left alone.
    std::unordered_set<std::string> GetHiddenLayers() const;

    // The layer `entity` draws on — its LayerComponent, or its prefab placement root's, since a
    // placement's layer belongs to the placement. Empty when it has none.
    std::string GetEntityLayer(Entity entity) const;
    // Whether `entity` is off-limits to editing because its layer is locked.
    bool IsEntityLocked(Entity entity) const;
    // The layer an entity without a LayerComponent is drawn on.
    std::string DefaultLayer() const;

    // --- Grid ---------------------------------------------------------------------------
    GridSettings& GetGrid();
    const GridSettings& GetGrid() const { return const_cast<EditorApplication*>(this)->GetGrid(); }
    // `world` snapped to the grid, or unchanged when snapping is off.
    Vector2 SnapToGrid(Vector2 world) const;

    // --- Commands -----------------------------------------------------------------------
    // Every mutation the editor makes to a document's world should go through here, so that it
    // can be undone. Reaching into World directly doesn't merely skip undo — it leaves the
    // history describing a state that no longer exists, which is worse than having no history.
    //
    // Commands apply immediately. Callers that mutate while iterating the entities they are
    // drawing must defer the call themselves (see HierarchyEditor's pendingAction_).
    void Execute(std::unique_ptr<EditorCommand> command);
    // The active document's history, or null when the active tab has no world.
    CommandHistory* GetHistory();
    void Undo();
    void Redo();
    // Group several commands into one undo step. Nestable; forwarded to the active history, and
    // silently ignored when there is no document, so callers need no null check.
    void BeginTransaction(const std::string& label);
    void EndTransaction();
    // A transaction that also merges repeats of the same edit, for a continuous drag.
    void BeginGesture(const std::string& label);
    void EndGesture();

    // --- Stable entity references -------------------------------------------------------
    // See EditorDocument. Commands store the id, never the Entity.
    // Assigns one on first use; 0 for INVALID_ENTITY.
    uint64_t StableIdOf(Entity entity);
    // The entity `id` currently refers to, or INVALID_ENTITY if it refers to nothing (because
    // an undo destroyed it, or the document was reloaded).
    Entity EntityForStableId(uint64_t id) const;
    // Points `id` at `entity`, for a command that has just recreated what it destroyed.
    // INVALID_ENTITY unbinds it.
    void RebindStableId(uint64_t id, Entity entity);

    const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const { return componentPlaceholders; }

    // --- Clipboard ----------------------------------------------------------------------
    // Entities are copied as serialized XML onto the *system* clipboard, so a copy carries
    // between open documents and between running instances of the editor, and survives a
    // reload. Cut is a copy and a delete in one undo step.
    void CopySelection();
    void CutSelection();
    // Pastes the clipboard with its first root at `at` (grid-snapped), the rest keeping their
    // offsets from it, onto the active layer. Returns the first pasted root.
    Entity Paste(Vector2 at);
    bool CanPaste() const;

    const std::vector<Entity>& GetSelectedEntities() const { return selectedEntities_; }
    void SelectEntity(Entity entity, bool additive = false);
    void ClearSelection();
    bool IsSelected(Entity entity) const;

    EditorCamera& GetEditorCamera() { return editorCamera_; }

    // Hierarchy-aware edits on the viewport world. Duplicate copies the whole subtree next to
    // the original (a placed prefab becomes a new placement with the same overrides) and
    // returns the copy's root; Delete removes the entity and everything under it.
    Entity DuplicateEntity(Entity entity);
    void DeleteEntity(Entity entity);
    // Creates an entity under `parent`, or at root level. Returns INVALID_ENTITY if refused.
    Entity CreateEntity(Entity parent = INVALID_ENTITY);
    // Hierarchy edits, so the Hierarchy panel's drag-and-drop is undoable like everything else.
    // Reparent to INVALID_ENTITY detaches to root level. Both reorders move `entity` to sit
    // beside `sibling` under the same parent.
    void Reparent(Entity entity, Entity parent);
    void ReorderBefore(Entity entity, Entity sibling);
    void ReorderAfter(Entity entity, Entity sibling);
    // Whether `entity` (INVALID_ENTITY: a new one) may sit at root level in the active
    // document. A prefab has exactly one root, so there it's only the existing root.
    bool CanBeRoot(Entity entity) const;

    // The scene the viewport shows and edits: the active document's. Null when nothing (or
    // an asset without a world) is open.
    Scene* GetViewportScene();

    // Documents (viewport tabs). Index -1 means none.
    // Opens the asset at `fullPath` in a tab, by its kind (see AssetKindOf); sounds can't be.
    void OpenAsset(const std::string& fullPath);
    // Opens Scenes/<sceneName>.xml (ScenePath).
    void OpenScene(const std::string& sceneName);
    void OpenPrefab(const std::string& fullPath);
    const std::vector<std::unique_ptr<EditorDocument>>& GetDocuments() const { return documents_; }
    int GetActiveDocument() const { return activeDocument_; }
    void SetActiveDocument(int index);
    void CloseDocument(int index);
    const EditorDocument* GetActiveDocumentInfo() const {
        const int index = GetActiveDocument();
        return index >= 0 ? GetDocuments()[index].get() : nullptr;
    }
    // Saves the active scene or prefab back to its file (other kinds save from their pane).
    bool SaveActiveDocument();
    // Places a prefab into the active tab's world at the editor camera. Returns the root entity.
    Entity InstantiatePrefab(const std::string& fullPath);
    // Writes a starter file of `kind` at `fullPath` and opens it. Sounds and textures can't
    // be made here. False if the file exists or can't be written.
    bool CreateAsset(AssetKind kind, const std::string& fullPath);
    // Writes the active scene or prefab to `fullPath` and opens that in the tab's place.
    // Other kinds are written by their content pane, which then calls ReplaceActiveDocument.
    bool SaveActiveDocumentAs(const std::string& fullPath);
    // Opens the asset at `fullPath` in place of the active tab.
    void ReplaceActiveDocument(const std::string& fullPath);
    // Packs `entity` and its subtree (in the active tab) into a new prefab at `fullPath` and
    // replaces them with a placement of it. False if the file exists or fails.
    bool CreatePrefabFromEntity(Entity entity, const std::string& fullPath);
    // Renames an open scene document's file to <newName>.xml beside it (the project's entry
    // scene follows). False, logged, if the name is bad or taken.
    bool RenameScene(Scene& scene, const std::string& newName);
   private:
    void OpenSceneFile(const std::string& fullPath);
    // The active document's grid, into its scene's editor metadata, so it saves with it.
    void StoreGrid(Scene& scene);
    ServiceLocator& registry_;
    std::unique_ptr<EditorUI> ui_;

    std::vector<ComponentPlaceholder> componentPlaceholders;
    std::vector<Entity> selectedEntities_;
    EditorCamera editorCamera_;
    bool openedEntryScene_ = false;
    // Layers/systems for prefab documents when no scene document is open (the entry scene).
    std::shared_ptr<Scene> fallbackHost_;

    // Per-document editing state. Keyed by the document's file path so it survives tab
    // switches, and dropped with the document. None of it is persisted.
    struct DocumentEditState {
        std::string activeLayer;
        std::unordered_map<std::string, LayerEditState> layers;
        GridSettings grid;

        // Recomputed rather than cached: GetLayerState hands out a mutable reference, so any
        // cached count would go stale the moment a caller flips a flag through it.
        bool AnySolo() const {
            for (const auto& [name, layer] : layers) {
                if (layer.solo) return true;
            }
            return false;
        }
    };
    mutable std::unordered_map<std::string, DocumentEditState> editState_;
    // The active document's state, created on demand. Falls back to a scratch entry when no
    // document is open so callers never get a null.
    DocumentEditState& EditState() const;

    // Keeps a document's stable-id bindings honest by unbinding an id the moment its entity is
    // destroyed, by whatever route — a command, a prefab respawn, Unpack. Without this, a
    // destroyed entity's index gets recycled and a command holding the old id would silently
    // resolve to whatever unrelated entity now occupies that slot.
    struct StableIdBinding : IWorldListener {
        EditorDocument* document = nullptr;
        void OnEntityDestroyed(Entity entity) override;
    };
    // One per document, parallel to documents_ and torn down with it.
    std::vector<std::unique_ptr<StableIdBinding>> stableIdBindings_;

    // Builds the context commands act through. Null when the active tab has no world.
    std::optional<CommandContext> CommandCtx();
    // Drops entities that no longer exist from the selection, after an undo or redo.
    void PruneSelection();
    // Records an entity the editor has just finished creating, so it can be undone. The work
    // is already done by the time this runs — callers need the new Entity back to keep
    // configuring it — which is why SpawnCommand's first Do() is deliberately a no-op.
    void RecordSpawn(Entity entity, const std::string& label);

    std::vector<std::unique_ptr<EditorDocument>> documents_;
    int activeDocument_ = -1;
    EditorDocument* ActiveDocument() const;
    int FindDocument(const std::string& fullPath) const;
    void AddDocument(std::unique_ptr<EditorDocument> doc);
    // The scene prefab documents take their layers and systems from.
    const Scene* HostScene();
    // The active prefab document's root entity, if it has one.
    Entity PrefabRoot() const;
    bool SavePrefabDocument(EditorDocument& doc);
    Entity DuplicateSubtree(World& world, Entity entity, Entity newParent);
    // Directory of the file the active tab saves to, for relative prefab paths.
    std::string ActiveOwnerDir() const;

    void RegisterComponentTypes();
};

}  // namespace Elysium

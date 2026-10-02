#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include "Core/AssetKind.h"
#include "Core/CommandHistory.h"
#include "Core/EditorCommand.h"
#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include "Core/Prefab.h"
#include "Interfaces/IService.h"

namespace Elysium {
class World;
class Scene;
}  // namespace Elysium

namespace Elysium::Services {

struct ComponentPlaceholder {
    std::function<void(Entity, Elysium::World*)> drawFunc;
    std::function<bool(Entity, Elysium::World*)> hasComponentFunc;
    std::function<void(Entity, Elysium::World*)> addComponentFunc;
    std::function<void(Entity, Elysium::World*)> removeComponentFunc;
    std::function<void(Entity, Elysium::World*)> resetComponentFunc;
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
    std::shared_ptr<Elysium::Scene> scene;  // scenes and prefabs only
    // Prefab only:
    std::unordered_map<Entity, int> localIds;     // entity -> <Entity id> in the file
    std::vector<Elysium::PrefabParameter> parameters;

    // Undo/redo for this tab, so each open scene has its own history and closing the tab
    // discards it. Scenes and prefabs only; a content pane has nothing to undo through here.
    Elysium::CommandHistory history;

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

class IEditorService : public IService {
   public:
    virtual Elysium::World* GetWorld() const = 0;

    // --- Layers (active document) ------------------------------------------------------
    // The layer being worked on: it filters the Hierarchy and receives painted prefabs.
    // Empty means no layer is focused, and the Hierarchy shows everything.
    virtual const std::string& GetActiveLayer() const = 0;
    virtual void SetActiveLayer(const std::string& layer) = 0;

    virtual LayerEditState& GetLayerState(const std::string& layer) = 0;

    // Effective visibility and lock, with solo resolved: once any layer is soloed, every layer
    // that isn't soloed counts as hidden.
    virtual bool IsLayerHidden(const std::string& layer) const = 0;
    virtual bool IsLayerLocked(const std::string& layer) const = 0;
    // Every layer that should not draw this frame. RenderSorter applies this to its own copy
    // of the layer list, so SceneLayer::isVisible in the scene is left alone.
    virtual std::unordered_set<std::string> GetHiddenLayers() const = 0;

    // The layer `entity` draws on — its LayerComponent, or its prefab placement root's, since a
    // placement's layer belongs to the placement. Empty when it has none.
    virtual std::string GetEntityLayer(Entity entity) const = 0;
    // Whether `entity` is off-limits to editing because its layer is locked.
    virtual bool IsEntityLocked(Entity entity) const = 0;

    // --- Grid ---------------------------------------------------------------------------
    virtual GridSettings& GetGrid() = 0;
    const GridSettings& GetGrid() const { return const_cast<IEditorService*>(this)->GetGrid(); }
    // `world` snapped to the grid, or unchanged when snapping is off.
    virtual Vector2 SnapToGrid(Vector2 world) const = 0;

    // --- Commands -----------------------------------------------------------------------
    // Every mutation the editor makes to a document's world should go through here, so that it
    // can be undone. Reaching into World directly doesn't merely skip undo — it leaves the
    // history describing a state that no longer exists, which is worse than having no history.
    //
    // Commands apply immediately. Callers that mutate while iterating the entities they are
    // drawing must defer the call themselves (see HierarchyEditor's pendingAction_).
    virtual void Execute(std::unique_ptr<Elysium::EditorCommand> command) = 0;
    // The active document's history, or null when the active tab has no world.
    virtual Elysium::CommandHistory* GetHistory() = 0;
    virtual void Undo() = 0;
    virtual void Redo() = 0;
    // Group several commands into one undo step. Nestable; forwarded to the active history, and
    // silently ignored when there is no document, so callers need no null check.
    virtual void BeginTransaction(const std::string& label) = 0;
    virtual void EndTransaction() = 0;
    // A transaction that also merges repeats of the same edit, for a continuous drag.
    virtual void BeginGesture(const std::string& label) = 0;
    virtual void EndGesture() = 0;

    // --- Stable entity references -------------------------------------------------------
    // See EditorDocument. Commands store the id, never the Entity.
    // Assigns one on first use; 0 for INVALID_ENTITY.
    virtual uint64_t StableIdOf(Entity entity) = 0;
    // The entity `id` currently refers to, or INVALID_ENTITY if it refers to nothing (because
    // an undo destroyed it, or the document was reloaded).
    virtual Entity EntityForStableId(uint64_t id) const = 0;
    // Points `id` at `entity`, for a command that has just recreated what it destroyed.
    // INVALID_ENTITY unbinds it.
    virtual void RebindStableId(uint64_t id, Entity entity) = 0;

    virtual const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const = 0;

    // --- Clipboard ----------------------------------------------------------------------
    // Entities are copied as serialized XML onto the *system* clipboard, so a copy carries
    // between open documents and between running instances of the editor, and survives a
    // reload. Cut is a copy and a delete in one undo step.
    virtual void CopySelection() = 0;
    virtual void CutSelection() = 0;
    // Pastes the clipboard with its first root at `at` (grid-snapped), the rest keeping their
    // offsets from it, onto the active layer. Returns the first pasted root.
    virtual Entity Paste(Vector2 at) = 0;
    virtual bool CanPaste() const = 0;

    virtual const std::vector<Entity>& GetSelectedEntities() const = 0;
    virtual void SelectEntity(Entity entity, bool additive = false) = 0;
    virtual void ClearSelection() = 0;
    virtual bool IsSelected(Entity entity) const = 0;

    virtual EditorCamera& GetEditorCamera() = 0;

    // Hierarchy-aware edits on the viewport world. Duplicate copies the whole subtree next to
    // the original (a placed prefab becomes a new placement with the same overrides) and
    // returns the copy's root; Delete removes the entity and everything under it.
    virtual Entity DuplicateEntity(Entity entity) = 0;
    virtual void DeleteEntity(Entity entity) = 0;
    // Creates an entity under `parent`, or at root level. Returns INVALID_ENTITY if refused.
    virtual Entity CreateEntity(Entity parent = INVALID_ENTITY) = 0;
    // Hierarchy edits, so the Hierarchy panel's drag-and-drop is undoable like everything else.
    // Reparent to INVALID_ENTITY detaches to root level. Both reorders move `entity` to sit
    // beside `sibling` under the same parent.
    virtual void Reparent(Entity entity, Entity parent) = 0;
    virtual void ReorderBefore(Entity entity, Entity sibling) = 0;
    virtual void ReorderAfter(Entity entity, Entity sibling) = 0;
    // Whether `entity` (INVALID_ENTITY: a new one) may sit at root level in the active
    // document. A prefab has exactly one root, so there it's only the existing root.
    virtual bool CanBeRoot(Entity entity) const = 0;

    // The scene the viewport shows and edits: the active document's. Null when nothing (or
    // an asset without a world) is open.
    virtual Elysium::Scene* GetViewportScene() = 0;

    // Documents (viewport tabs). Index -1 means none.
    // Opens the asset at `fullPath` in a tab, by its kind (see AssetKindOf); sounds can't be.
    virtual void OpenAsset(const std::string& fullPath) = 0;
    virtual void OpenScene(const std::string& sceneName) = 0;
    virtual void OpenPrefab(const std::string& fullPath) = 0;
    virtual const std::vector<std::unique_ptr<EditorDocument>>& GetDocuments() const = 0;
    virtual int GetActiveDocument() const = 0;
    virtual void SetActiveDocument(int index) = 0;
    virtual void CloseDocument(int index) = 0;
    const EditorDocument* GetActiveDocumentInfo() const {
        const int index = GetActiveDocument();
        return index >= 0 ? GetDocuments()[index].get() : nullptr;
    }
    // Saves the active scene or prefab back to its file (other kinds save from their pane).
    virtual bool SaveActiveDocument() = 0;
    // Places a prefab into the active tab's world at the editor camera. Returns the root entity.
    virtual Entity InstantiatePrefab(const std::string& fullPath) = 0;
    // Writes a starter file of `kind` at `fullPath` and opens it. Sounds and textures can't
    // be made here. False if the file exists or can't be written.
    virtual bool CreateAsset(AssetKind kind, const std::string& fullPath) = 0;
    // Writes the active scene or prefab to `fullPath` and opens that in the tab's place.
    // Other kinds are written by their content pane, which then calls ReplaceActiveDocument.
    virtual bool SaveActiveDocumentAs(const std::string& fullPath) = 0;
    // Opens the asset at `fullPath` in place of the active tab.
    virtual void ReplaceActiveDocument(const std::string& fullPath) = 0;
    // Packs `entity` and its subtree (in the active tab) into a new prefab at `fullPath` and
    // replaces them with a placement of it. False if the file exists or fails.
    virtual bool CreatePrefabFromEntity(Entity entity, const std::string& fullPath) = 0;
};

}  // namespace Elysium::Services

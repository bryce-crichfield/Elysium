#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include "Core/AssetKind.h"
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

    bool IsPrefab() const { return kind == AssetKind::Prefab; }
    bool IsScene() const { return kind == AssetKind::Scene; }
    // Scenes and prefabs: the Hierarchy and Inspector work on this document's entities.
    bool HasWorld() const { return scene != nullptr; }
};

class IEditorService : public IService {
   public:
    virtual Elysium::World* GetWorld() const = 0;

    virtual const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const = 0;

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

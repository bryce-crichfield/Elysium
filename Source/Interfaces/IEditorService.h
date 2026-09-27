#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
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

// A scene or prefab file open for editing in its own viewport tab. Its entities live in a
// private Scene loaded from disk, never on the game's stack, so editing and playing never
// share state. A prefab borrows a scene's layers and systems so it renders the same way.
struct EditorDocument {
    enum class Kind { Scene, Prefab };
    Kind kind = Kind::Prefab;
    std::string fullPath;
    std::string title;
    std::shared_ptr<Elysium::Scene> scene;
    // Prefab only:
    std::unordered_map<Entity, int> localIds;     // entity -> <Entity id> in the file
    std::vector<Elysium::PrefabParameter> parameters;

    bool IsPrefab() const { return kind == Kind::Prefab; }
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

    // The scene the Scene panel inspects: the active document's. Null when nothing is open.
    virtual Elysium::Scene* GetInspectedScene() = 0;

    // The scene the viewport shows and edits: the active document's. Null when nothing is open.
    virtual Elysium::Scene* GetViewportScene() = 0;

    // Documents (viewport tabs). Index -1 means none.
    virtual void OpenScene(const std::string& sceneName) = 0;
    virtual void OpenPrefab(const std::string& fullPath) = 0;
    virtual const std::vector<std::unique_ptr<EditorDocument>>& GetDocuments() const = 0;
    virtual int GetActiveDocument() const = 0;
    virtual void SetActiveDocument(int index) = 0;
    virtual void CloseDocument(int index) = 0;
    // Saves the active document back to its file.
    virtual bool SaveActiveDocument() = 0;
    // Places a prefab into the active tab's world at the editor camera. Returns the root entity.
    virtual Entity InstantiatePrefab(const std::string& fullPath) = 0;
    // Writes a minimal prefab (one root entity) into `directory` and opens it.
    virtual void CreatePrefab(const std::string& directory) = 0;
    // Writes `entity` and its subtree (in the active tab) as a new prefab at `fullPath` and
    // opens it. The original entities are left as they are. False if the file exists or fails.
    virtual bool CreatePrefabFromEntity(Entity entity, const std::string& fullPath) = 0;
};

}  // namespace Elysium::Services

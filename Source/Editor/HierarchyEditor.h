#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Core/Editor.h"
#include "Core/Entity.h"

namespace Elysium {
class World;
}

namespace Elysium::Services {
class IEditorService;
}

namespace Elysium {

// The entity browser: a searchable parent/child tree (or flat list) with drag-and-drop
// reparenting and reordering. The selected entity is edited in the Inspector panel.
class HierarchyEditor : public Editor {
   public:
    static constexpr const char* Title = "Hierarchy";

    explicit HierarchyEditor(ServiceLocator& services);

    void Draw() override;

   private:
    // Structural edits (duplicate/delete) chosen while drawing, applied once drawing is done.
    std::function<void()> pendingAction_;

    void DrawToolbar(Services::IEditorService& service);
    void DrawLuaFilter();
    void DrawEntityList(Services::IEditorService& service);
    void DrawHierarchyTree(Services::IEditorService& service);
    void DrawHierarchyNode(Services::IEditorService& service, Entity entity);
    // Renders a thin drop zone used for reordering and reparenting via drag-and-drop.
    // parent: the entity whose childrenMap_ will receive the drop (INVALID_ENTITY = root level).
    // beforeSibling: the sibling to insert before; INVALID_ENTITY = append at end.
    void DrawInsertionZone(Services::IEditorService& service, Entity parent, Entity beforeSibling);
    void DrawEntityContextMenu(Services::IEditorService& service, Entity entity);
    void DrawCreateEntityMenu(Services::IEditorService& service);
    void DeferOpenPrefab(Services::IEditorService& service, const World& world, Entity entity);
    // "Pack Prefab" dialog: name + folder for a new prefab made from an entity's subtree.
    void BeginCreatePrefab(Services::IEditorService& service, Entity entity);
    void DrawCreatePrefabDialog(Services::IEditorService& service);

    // True when `entity` passes the name search, the Lua filter, and the focused layer (set in
    // the Viewport's layer drawer — empty means every layer).
    bool PassesFilters(const Services::IEditorService& service, const World& world, Entity entity) const;

    bool showHierarchyView_ = true;
    bool showLuaFilter_ = false;

    char searchBuffer_[128] = "";
    char luaFilterBuffer_[1024] = "function filter(e)\n  return true\nend";
    bool luaFilterActive_ = false;
    std::vector<Entity> filteredEntities_;
    std::optional<bool> openRequest_;  // expand/collapse all, applied for one frame

    struct {
        Entity source = INVALID_ENTITY;  // entity the prefab is made from
        bool open = false;               // open next frame (OpenPopup can't run inside the context menu)
        char name[128] = "";
        std::string folder;                // relative to the project root
        std::vector<std::string> folders;  // project folders, gathered when the dialog opens
    } prefabDialog_;
};

}  // namespace Elysium

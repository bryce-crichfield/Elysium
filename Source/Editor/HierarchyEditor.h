#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Editor.h"
#include "Core/Entity.h"

namespace Elysium {
class World;
class EditorApplication;
}

namespace Elysium {

// The entity browser: a searchable parent/child tree (or flat list) with drag-and-drop
// reparenting and reordering. The selected entity is edited in the Inspector panel.
class HierarchyEditor : public Editor {
   public:
    static constexpr const char* Title = "Hierarchy";

    explicit HierarchyEditor(EditorApplication& editor);

    void Draw() override;

   private:
    // Structural edits (duplicate/delete) chosen while drawing, applied once drawing is done.
    std::function<void()> pendingAction_;

    void DrawToolbar(EditorApplication& editor);
    void DrawLuaFilter();
    void DrawEntityList(EditorApplication& editor);
    void DrawHierarchyTree(EditorApplication& editor);
    void DrawHierarchyNode(EditorApplication& editor, Entity entity);

    // --- Row selection --------------------------------------------------------------------
    // Ctrl-click toggles one row; shift-click takes everything between the last row clicked and
    // this one. A range is over *rows as displayed*, not entity ids or world order, so it means
    // what the eye expects: collapsed subtrees and grouped placements count as the block they
    // occupy, and a filtered-out entity is never caught in a range that looks like it skipped
    // over it. So rows are recorded as they are drawn and the range is resolved at the end of the
    // frame, once the list is complete -- the anchor may sit below the row that was clicked, and
    // at click time that part of the list does not exist yet.
    std::vector<Entity> visibleRows_;
    Entity selectionAnchor_ = INVALID_ENTITY;
    Entity pendingRangeTo_ = INVALID_ENTITY;
    bool pendingRangeAdditive_ = false;

    // Records `entity` as drawn at this point in the list. Every clickable row calls it.
    void RecordRow(Entity entity);
    // What a click on a row does, given the modifiers held.
    void HandleRowClick(EditorApplication& editor, Entity entity);
    // Applies a deferred shift-click once visibleRows_ holds the whole list.
    void ApplyPendingRange(EditorApplication& editor);

    // --- Placement grouping ---------------------------------------------------------------
    // A dungeon floor is a hundred-odd identical Floor placements. Listed one per row they bury
    // the handful of entities that actually differ, and the layer filter is no help because they
    // all sit on the same layer. So root-level placements of the same prefab on the same layer
    // collapse into a single expandable row, which is purely a view of the same entities:
    // nothing about the scene format changes.

    // One row of the root list: a lone entity, or several that collapsed together.
    struct RootRow {
        std::string label;           // the prefab's name, for a group
        std::string layer;           // which layer they are on, for the tooltip
        std::vector<Entity> members;
    };
    // Root entities as rows, in world order, with groupable placements merged.
    std::vector<RootRow> BuildRootRows(EditorApplication& editor) const;
    void DrawGroupNode(EditorApplication& editor, const RootRow& row);
    // Renders a thin drop zone used for reordering and reparenting via drag-and-drop.
    // parent: the entity whose childrenMap_ will receive the drop (INVALID_ENTITY = root level).
    // beforeSibling: the sibling to insert before; INVALID_ENTITY = append at end.
    void DrawInsertionZone(EditorApplication& editor, Entity parent, Entity beforeSibling);
    void DrawEntityContextMenu(EditorApplication& editor, Entity entity);
    void DrawCreateEntityMenu(EditorApplication& editor);
    void DeferOpenPrefab(EditorApplication& editor, const World& world, Entity entity);
    // "Pack Prefab" dialog: name + folder for a new prefab made from an entity's subtree.
    void BeginCreatePrefab(EditorApplication& editor, Entity entity);
    void DrawCreatePrefabDialog(EditorApplication& editor);

    // True when `entity` passes the name search, the Lua filter, and the focused layer (set in
    // the Viewport's layer drawer — empty means every layer).
    bool PassesFilters(const EditorApplication& editor, const World& world, Entity entity) const;

    bool showHierarchyView_ = true;
    bool showLuaFilter_ = false;
    bool groupPlacements_ = true;

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

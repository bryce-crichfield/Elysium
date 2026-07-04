#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Core/Editor.h"
#include "Core/Entity.h"

namespace Elysium {
class World;
}

namespace Elysium::Services {
class EditorService;
}

namespace Elysium {

class WorldEditor : public Editor {
   public:
    WorldEditor();

    void Draw(Application& app) override;

   private:
    void DrawEntityToolbar(Services::EditorService& service);
    void DrawEntityList(Services::EditorService& service);
    void DrawHierarchyTree(Services::EditorService& service);
    void DrawHierarchyNode(Services::EditorService& service, Entity entity);
    // Renders a thin drop zone used for reordering and reparenting via drag-and-drop.
    // parent: the entity whose childrenMap_ will receive the drop (INVALID_ENTITY = root level).
    // beforeSibling: the sibling to insert before; INVALID_ENTITY = append at end.
    void DrawInsertionZone(Services::EditorService& service, Entity parent, Entity beforeSibling);
    void DrawEntityContextMenu(Services::EditorService& service, Entity entity);
    void DrawInspectorToolbar(Services::EditorService& service);
    void DrawInspectorPanel(Services::EditorService& service);
    void DrawComponentPanel(Services::EditorService& service, size_t placeholderIndex);

    // Panel state
    float leftPanelWidth_ = 240.0f;
    bool isDraggingSplitter_ = false;
    bool showHierarchyView_ = true;

    std::string filterScriptBuffer_;
    std::vector<Entity> filteredEntities_;

    // Component deletion (deferred to avoid iterator invalidation)
    std::string componentToDelete_;
};

}  // namespace Elysium

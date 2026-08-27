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
class IEditorService;
}

namespace Elysium {

class WorldEditor : public Editor {
   public:
    explicit WorldEditor(ServiceLocator& services);

    void Draw() override;

   private:
    void DrawEntityToolbar(Services::IEditorService& service);
    void DrawEntityList(Services::IEditorService& service);
    void DrawHierarchyTree(Services::IEditorService& service);
    void DrawHierarchyNode(Services::IEditorService& service, Entity entity);
    // Renders a thin drop zone used for reordering and reparenting via drag-and-drop.
    // parent: the entity whose childrenMap_ will receive the drop (INVALID_ENTITY = root level).
    // beforeSibling: the sibling to insert before; INVALID_ENTITY = append at end.
    void DrawInsertionZone(Services::IEditorService& service, Entity parent, Entity beforeSibling);
    void DrawEntityContextMenu(Services::IEditorService& service, Entity entity);
    void DrawInspectorToolbar(Services::IEditorService& service);
    void DrawInspectorPanel(Services::IEditorService& service);
    void DrawComponentPanel(Services::IEditorService& service, size_t placeholderIndex);

    // The Inspector shows a single "primary" entity — the most recently selected one.
    // Multi-select is tracked by EditorService but has no dedicated UI yet.
    Entity GetPrimarySelection(Services::IEditorService& service) const;

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

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
    void DrawEntityContextMenu(Services::EditorService& service, Entity entity);
    void DrawInspectorToolbar(Services::EditorService& service);
    void DrawInspectorPanel(Services::EditorService& service);
    void DrawComponentPanel(Services::EditorService& service, size_t placeholderIndex);

    // Panel state
    float leftPanelWidth_ = 240.0f;
    bool isDraggingSplitter_ = false;
    bool showHierarchyView_ = false;

    std::string filterScriptBuffer_;
    std::vector<Entity> filteredEntities_;

    // Component deletion (deferred to avoid iterator invalidation)
    std::string componentToDelete_;
};

}  // namespace Elysium

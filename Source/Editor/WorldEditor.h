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
class WorldService;
}

namespace Elysium {

class WorldEditor : public Editor {
   public:
    WorldEditor();

    void Draw(Application& app) override;

   private:
    void DrawEntityToolbar(Services::WorldService& service);
    void DrawEntityList(Services::WorldService& service);
    void DrawInspectorToolbar(Services::WorldService& service);
    void DrawInspectorPanel(Services::WorldService& service);
    void DrawComponentPanel(Services::WorldService& service, size_t placeholderIndex);

    // Panel state
    float leftPanelWidth_ = 240.0f;
    bool isDraggingSplitter_ = false;

    std::string filterScriptBuffer_;
    std::vector<Entity> filteredEntities_;

    // Component deletion (deferred to avoid iterator invalidation)
    std::string componentToDelete_;
};

}  // namespace Elysium

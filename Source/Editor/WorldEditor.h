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

enum class FilterLogicalOperator { AND,
                                   OR };

struct EntityFilter {
    std::string componentName;
    bool negate = false;
    FilterLogicalOperator logicalOperator = FilterLogicalOperator::OR;
    std::function<bool(Entity, World*)> predicate;

    EntityFilter() = default;
    EntityFilter(const std::string& name, bool neg, FilterLogicalOperator op, std::function<bool(Entity, World*)> pred)
        : componentName(name), negate(neg), logicalOperator(op), predicate(pred) {}

    bool Evaluate(Entity entity, World* world) const {
        return negate ? !predicate(entity, world) : predicate(entity, world);
    }
};

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

    // Filter state
    std::vector<EntityFilter> filters_;
    bool filtersCollapsed_ = true;
    std::string searchFilter_;

    // Component deletion (deferred to avoid iterator invalidation)
    std::string componentToDelete_;
};

}  // namespace Elysium

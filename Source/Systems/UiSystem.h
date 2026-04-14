#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include <functional>

namespace Elysium::Systems {

// Processes entities tagged with UiComponent as a hierarchy rather than a
// flat list. Traverses roots (UiComponent with no valid UI parent) depth-first,
// visiting each entity before its children.
//
// Current behaviour: establishes traversal order; invokes OnVisit per entity.
// Future extension point: accumulate parent transforms for layout / padding.
class UiSystem : public System {
   public:
    UiSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;

   private:
    void TraverseTree(Entity root, std::function<void(Entity)> visitor);
};

}  // namespace Elysium::Systems

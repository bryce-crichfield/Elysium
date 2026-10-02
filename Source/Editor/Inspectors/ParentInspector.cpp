#include "Core/Components/ParentComponent.h"

#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Widgets/Widgets.h"
#include "imgui.h"

namespace Elysium {

void InspectParent(ParentComponent& c, Entity e, ServiceLocator& services) {
    PropertyLabel("Parent");
    ImGui::Text("%s (id=%zu)", c.targetName.c_str(), c.parent);

    PropertyLabel("Child Index");
    ImGui::Text("%u", c.childIndex);
}

}  // namespace Elysium

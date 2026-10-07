#include "Core/Components/PolygonComponent.h"

#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Widgets/Widgets.h"
#include "imgui.h"

namespace Elysium {

void InspectPolygon(PolygonComponent& c, Entity e, ServiceLocator& services) {
    PropertyLabel("Points");
    ImGui::Text("%d", (int)c.points.size());
}

}  // namespace Elysium

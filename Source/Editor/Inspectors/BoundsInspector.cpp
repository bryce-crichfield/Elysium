#include "Core/Components/BoundsComponent.h"

#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Widgets/Widgets.h"
#include "imgui.h"

namespace Elysium {

void InspectBounds(BoundsComponent& c, Entity e, ServiceLocator& services) {
    PropertyLabel("X");
    ImGui::Text("%.1f", c.bounds.x);
    PropertyLabel("Y");
    ImGui::Text("%.1f", c.bounds.y);
    PropertyLabel("Width");
    ImGui::Text("%.1f", c.bounds.width);
    PropertyLabel("Height");
    ImGui::Text("%.1f", c.bounds.height);

    PropertyLabel("Is Dragging");
    ImGui::Checkbox("##IsDragging", &c.isDragging);

    float color[4] = {c.debugColor.r / 255.0f, c.debugColor.g / 255.0f,
                      c.debugColor.b / 255.0f, c.debugColor.a / 255.0f};
    PropertyLabel("Debug Color");
    if (ImGui::ColorEdit4("##DebugColor", color)) {
        c.debugColor = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255),
                             (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255)};
    }

    ImGui::Spacing();
    MutedText("Bounds are computed automatically by RenderSystem.");
}

}  // namespace Elysium

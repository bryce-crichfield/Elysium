#include "Core/Components/MovementComponent.h"

#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Style/Palette.h"
#include "Editor/Widgets/Widgets.h"
#include "Editor/Editor.h"
#include "imgui.h"

namespace Elysium {

void InspectMovement(MovementComponent& c, Entity e, ServiceLocator& services) {
    // state display + manual override
    const char* stateNames[] = { "Idle", "Moving", "Waiting" };
    int stateIdx = static_cast<int>(c.state);
    PropertyLabel("State");
    if (ImGui::Combo("##State", &stateIdx, stateNames, 3))
        c.state = static_cast<MovementState>(stateIdx);

    // goal
    PropertyLabel("Goal");
    ImGui::DragFloat2("##Goal", &c.goal.x, 1.0f);

    // wait timer — only meaningful when Waiting
    ImGui::BeginDisabled(c.state != MovementState::Waiting);
    PropertyLabel("Wait Time (ms)");
    ImGui::DragInt("##WaitTimeMs", &c.waitTimeMs, 1.0f, 0, 5000);
    ImGui::EndDisabled();

    // stuck state
    PropertyLabel("Stuck Retry Count");
    ImGui::DragInt("##StuckRetryCount", &c.stuckRetryCount, 1.0f, 0, 10);

    PropertyLabel("Stuck Accum (ms)");
    ImGui::Text("%d", c.stuckCheckAccumMs);

    PropertyLabel("Last Position");
    ImGui::Text("(%.1f, %.1f)", c.lastPosition.x, c.lastPosition.y);

    SectionHeader("Waypoints");

    // waypoints
    ImGui::TextDisabled("%zu total, current %d",
        c.waypoints.size(), c.currentWaypointIndex);

    if (ImGui::Button("Clear Waypoints"))
        c.waypoints.clear();

    for (size_t i = 0; i < c.waypoints.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        // highlight the active waypoint
        bool isCurrent = (static_cast<int>(i) == c.currentWaypointIndex);
        if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, Editor::Palette().Success);
        const std::string label = "[" + std::to_string(i) + "]" + (isCurrent ? "  " ICON_FA_ARROW_LEFT : "");
        PropertyLabel(label.c_str());
        ImGui::DragFloat3("##wp", &c.waypoints[i].x, 1.0f);
        if (isCurrent) ImGui::PopStyleColor();

        ImGui::PopID();
    }
}

}  // namespace Elysium

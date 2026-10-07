#include "Core/Components/UiComponent.h"

#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Widgets/Widgets.h"
#include "imgui.h"

namespace Elysium {

void InspectUi(UiComponent& c, Entity e, ServiceLocator& services) {
    const char* containerItems[] = { "None", "Vertical", "Horizontal" };
    int current = static_cast<int>(c.containerType);
    PropertyLabel("Container");
    if (ImGui::Combo("##Container", &current, containerItems, 3))
        c.containerType = static_cast<UiContainerType>(current);
    if (c.containerType != UiContainerType::None) {
        PropertyLabel("Gap");
        ImGui::DragFloat("##Gap", &c.gap, 1.0f, 0.0f, 500.0f);
    }

    const char* alignItems[] = { "Start", "Middle", "End" };
    int ah = static_cast<int>(c.alignHorizontal);
    int av = static_cast<int>(c.alignVertical);
    PropertyLabel("Align H");
    if (ImGui::Combo("##AlignH", &ah, alignItems, 3))
        c.alignHorizontal = static_cast<UiAlignment>(ah);
    PropertyLabel("Align V");
    if (ImGui::Combo("##AlignV", &av, alignItems, 3))
        c.alignVertical = static_cast<UiAlignment>(av);
}

}  // namespace Elysium

#include "Components/UiComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"
#include <cstring>

namespace Elysium {

static UiContainerType ParseContainerType(const char* s) {
    if (!s) return UiContainerType::None;
    if (strcmp(s, "Vertical")   == 0) return UiContainerType::Vertical;
    if (strcmp(s, "Horizontal") == 0) return UiContainerType::Horizontal;
    return UiContainerType::None;
}

static const char* ContainerTypeName(UiContainerType t) {
    switch (t) {
        case UiContainerType::Vertical:   return "Vertical";
        case UiContainerType::Horizontal: return "Horizontal";
        default:                          return "None";
    }
}

static UiAlignment ParseAlignment(const char* s, UiAlignment fallback = UiAlignment::Start) {
    if (!s) return fallback;
    if (strcmp(s, "Middle") == 0) return UiAlignment::Middle;
    if (strcmp(s, "End")    == 0) return UiAlignment::End;
    return UiAlignment::Start;
}

static const char* AlignmentName(UiAlignment a) {
    switch (a) {
        case UiAlignment::Middle: return "Middle";
        case UiAlignment::End:    return "End";
        default:                  return "Start";
    }
}

void UiComponent::LoadXml(UiComponent& c, tinyxml2::XMLElement* el) {
    c.containerType  = ParseContainerType(el->Attribute("containerType"));
    el->QueryFloatAttribute("gap", &c.gap);
    c.alignHorizontal = ParseAlignment(el->Attribute("alignHorizontal"));
    c.alignVertical   = ParseAlignment(el->Attribute("alignVertical"));
}

void UiComponent::SaveXml(const UiComponent& c, XMLBuilder& builder) {
    auto elem = builder.AddElement("UiComponent");
    if (c.containerType != UiContainerType::None)
        elem.SetAttribute("containerType", ContainerTypeName(c.containerType));
    if (c.gap != 0.0f)
        elem.SetAttribute("gap", c.gap);
    if (c.alignHorizontal != UiAlignment::Start)
        elem.SetAttribute("alignHorizontal", AlignmentName(c.alignHorizontal));
    if (c.alignVertical != UiAlignment::Start)
        elem.SetAttribute("alignVertical", AlignmentName(c.alignVertical));
}

void UiComponent::Inspect(UiComponent& c, Entity) {
    const char* containerItems[] = { "None", "Vertical", "Horizontal" };
    int current = static_cast<int>(c.containerType);
    if (ImGui::Combo("Container", &current, containerItems, 3))
        c.containerType = static_cast<UiContainerType>(current);
    if (c.containerType != UiContainerType::None)
        ImGui::DragFloat("Gap", &c.gap, 1.0f, 0.0f, 500.0f);

    const char* alignItems[] = { "Start", "Middle", "End" };
    int ah = static_cast<int>(c.alignHorizontal);
    int av = static_cast<int>(c.alignVertical);
    if (ImGui::Combo("Align H", &ah, alignItems, 3))
        c.alignHorizontal = static_cast<UiAlignment>(ah);
    if (ImGui::Combo("Align V", &av, alignItems, 3))
        c.alignVertical = static_cast<UiAlignment>(av);
}

void UiComponent::BindLua(sol::usertype<UiComponent>&) {}

REGISTER_COMPONENT(UiComponent);

}  // namespace Elysium

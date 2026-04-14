#pragma once
#include "Core/Component.h"

namespace Elysium {

    enum class UiContainerType { None, Vertical, Horizontal };
    enum class UiAlignment { Start, Middle, End };

    struct UiComponent {
        UiContainerType containerType = UiContainerType::None;
        float gap = 0.0f;
        // Cross-axis alignment (perpendicular to the layout direction).
        // For Vertical containers: horizontal alignment of children.
        // For Horizontal containers: vertical alignment of children.
        // When containerType is None, these align the element itself within its parent.
        UiAlignment alignHorizontal = UiAlignment::Start;
        UiAlignment alignVertical   = UiAlignment::Start;

        static constexpr const char* Name() { return "Ui"; }
        static constexpr const char* XmlTag() { return "UiComponent"; }

        static void LoadXml(UiComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const UiComponent& c, XMLBuilder& builder);
        static void Inspect(UiComponent& c, Entity e);
        static void BindLua(sol::usertype<UiComponent>& ut);
    };
}

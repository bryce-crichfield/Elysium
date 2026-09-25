#pragma once
#include "Core/Component.h"
#include "Core/Graphics.h"

namespace Elysium {
    struct LineComponent {
        float x1, y1, x2, y2;  // local-space offsets from entity position
        float thickness;

        LineComponent(float x1 = 0.0f, float y1 = 0.0f, float x2 = 50.0f, float y2 = 0.0f,
                      float thickness = 1.0f);

        static constexpr const char* Name() { return "Line"; }
        static constexpr const char* XmlTag() { return "LineComponent"; }

        static void LoadXml(LineComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const LineComponent& c, XMLBuilder& builder);
        static void Inspect(LineComponent& c, Entity e, ServiceLocator& services);
        static void BindLua(sol::usertype<LineComponent>& ut);
        static void SetFromLua(LineComponent& c, sol::object v);
    };
}

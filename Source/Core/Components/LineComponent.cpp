#include "Core/Components/LineComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"

namespace Elysium {
    LineComponent::LineComponent(float x1, float y1, float x2, float y2, float thickness)
        : x1(x1), y1(y1), x2(x2), y2(y2), thickness(thickness) {}

    void LineComponent::SaveXml(const LineComponent& c, XMLBuilder& builder) {
        builder.AddElement("LineComponent")
            .SetAttribute("x1", c.x1)
            .SetAttribute("y1", c.y1)
            .SetAttribute("x2", c.x2)
            .SetAttribute("y2", c.y2)
            .SetAttribute("thickness", c.thickness);
    }

    void LineComponent::LoadXml(LineComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.x1 = el->FloatAttribute("x1", 0.0f);
        c.y1 = el->FloatAttribute("y1", 0.0f);
        c.x2 = el->FloatAttribute("x2", 50.0f);
        c.y2 = el->FloatAttribute("y2", 0.0f);
        c.thickness = el->FloatAttribute("thickness", 1.0f);
    }

    FieldList LineComponent::Fields() {
        return {
            Field("X1", &LineComponent::x1, "x1"),
            Field("Y1", &LineComponent::y1, "y1"),
            Field("X2", &LineComponent::x2, "x2"),
            Field("Y2", &LineComponent::y2, "y2"),
            Field("Thickness", &LineComponent::thickness, "thickness").Speed(0.1f).Range(0.1f, 50.0f),
        };
    }

    void LineComponent::BindLua(sol::usertype<LineComponent>& ut) {
        ut["x1"] = &LineComponent::x1;
        ut["y1"] = &LineComponent::y1;
        ut["x2"] = &LineComponent::x2;
        ut["y2"] = &LineComponent::y2;
        ut["thickness"] = &LineComponent::thickness;
    }

    void LineComponent::SetFromLua(LineComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.x1 = t.get_or("x1", c.x1);
            c.y1 = t.get_or("y1", c.y1);
            c.x2 = t.get_or("x2", c.x2);
            c.y2 = t.get_or("y2", c.y2);
            c.thickness = t.get_or("thickness", c.thickness);
        }
    }

    REGISTER_COMPONENT(LineComponent);
}

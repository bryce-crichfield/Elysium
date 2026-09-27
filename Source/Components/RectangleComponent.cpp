#include "Components/RectangleComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    RectangleComponent::RectangleComponent(float width, float height, float cornerRadius)
        : width(width), height(height), cornerRadius(cornerRadius) {}

    void RectangleComponent::SaveXml(const RectangleComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("RectangleComponent")
            .SetAttribute("width", c.width)
            .SetAttribute("height", c.height)
            .SetAttribute("cornerRadius", c.cornerRadius);
        if (c.originX != 0.5f || c.originY != 0.5f) b.SetAttribute("originX", c.originX).SetAttribute("originY", c.originY);
    }

    void RectangleComponent::LoadXml(RectangleComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.width = el->FloatAttribute("width", 100.0f);
        c.height = el->FloatAttribute("height", 100.0f);
        c.cornerRadius = el->FloatAttribute("cornerRadius", 0.0f);
        c.originX = el->FloatAttribute("originX", 0.5f);
        c.originY = el->FloatAttribute("originY", 0.5f);
    }

    FieldList RectangleComponent::Fields() {
        return {
            Field("Width", &RectangleComponent::width, "width").Range(1.0f, 1000.0f),
            Field("Height", &RectangleComponent::height, "height").Range(1.0f, 1000.0f),
            Field("Corner Radius", &RectangleComponent::cornerRadius, "cornerRadius").Speed(0.01f).Range(0.0f, 1.0f),
            Field("Origin X", &RectangleComponent::originX, "originX").Speed(0.01f).Range(0.0f, 1.0f),
            Field("Origin Y", &RectangleComponent::originY, "originY").Speed(0.01f).Range(0.0f, 1.0f),
        };
    }

    void RectangleComponent::BindLua(sol::usertype<RectangleComponent>& ut) {
        ut["width"] = &RectangleComponent::width;
        ut["height"] = &RectangleComponent::height;
        ut["cornerRadius"] = &RectangleComponent::cornerRadius;
        ut["originX"] = &RectangleComponent::originX;
        ut["originY"] = &RectangleComponent::originY;
    }

    void RectangleComponent::SetFromLua(RectangleComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.width = t.get_or("width", c.width);
            c.height = t.get_or("height", c.height);
            c.cornerRadius = t.get_or("cornerRadius", c.cornerRadius);
        }
    }

    REGISTER_COMPONENT(RectangleComponent);
}

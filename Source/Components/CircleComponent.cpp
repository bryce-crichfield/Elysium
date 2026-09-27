#include "Components/CircleComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    CircleComponent::CircleComponent(float r) : radius(r) {}

    void CircleComponent::SaveXml(const CircleComponent& c, XMLBuilder& builder) {
        builder.AddElement("CircleComponent")
            .SetAttribute("radius", c.radius);
    }

    void CircleComponent::LoadXml(CircleComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.radius = el->FloatAttribute("radius", 50.0f);
    }

    FieldList CircleComponent::Fields() {
        return {
            Field("Radius", &CircleComponent::radius, "radius").Range(1.0f, 1000.0f),
        };
    }

    void CircleComponent::BindLua(sol::usertype<CircleComponent>& ut) {
        ut["radius"] = &CircleComponent::radius;
    }

    void CircleComponent::SetFromLua(CircleComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.radius = t.get_or("radius", c.radius);
        }
    }

    REGISTER_COMPONENT(CircleComponent);
}

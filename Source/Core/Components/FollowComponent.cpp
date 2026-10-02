#include "Core/Components/FollowComponent.h"
#include "Core/ComponentRegistry.h"

namespace Elysium {
    void FollowComponent::LoadXml(FollowComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        el->QueryFloatAttribute("followSpeed", &c.speed);
        el->QueryFloatAttribute("offsetX", &c.offsetX);
        el->QueryFloatAttribute("offsetY", &c.offsetY);
    }

    FieldList FollowComponent::Fields() {
        return {
            Field("Follow Speed", &FollowComponent::speed, "followSpeed").Speed(0.1f),
            Field("Offset X", &FollowComponent::offsetX, "offsetX"),
            Field("Offset Y", &FollowComponent::offsetY, "offsetY"),
        };
    }

    void FollowComponent::BindLua(sol::usertype<FollowComponent>& ut) {
        ut["speed"]   = &FollowComponent::speed;
        ut["offsetX"] = &FollowComponent::offsetX;
        ut["offsetY"] = &FollowComponent::offsetY;
    }

    void FollowComponent::SetFromLua(FollowComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.speed   = t.get_or("speed",   c.speed);
            c.offsetX = t.get_or("offsetX", c.offsetX);
            c.offsetY = t.get_or("offsetY", c.offsetY);
        }
    }

    REGISTER_COMPONENT(FollowComponent);
}

#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct FollowComponent {
        float speed = 0.0f;   // <=0 means instant snap to parent; >0 is lerp factor
        float offsetX = 0.0f;
        float offsetY = 0.0f;

        static constexpr const char* Name() { return "Follow"; }
        static constexpr InspectorOrder Order = InspectorOrder::Physics;
        static constexpr const char* XmlTag() { return "FollowComponent"; }

        static void LoadXml(FollowComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static FieldList Fields();
        static void BindLua(sol::usertype<FollowComponent>& ut);
        static void SetFromLua(FollowComponent& c, sol::object v);
    };
}

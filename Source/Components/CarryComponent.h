#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    struct CarryComponent {
        std::string resourceType;
        float amount;
        float capacity;

        CarryComponent(float cap = 50.0f)
            : resourceType(""), amount(0.0f), capacity(cap) {}

        static constexpr const char* Name() { return "Carry"; }
        static constexpr const char* XmlTag() { return "CarryComponent"; }

        static void LoadXml(CarryComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(CarryComponent& c, Entity e);
        static void BindLua(sol::usertype<CarryComponent>& ut);
        static void SetFromLua(CarryComponent& c, sol::object v);
    };
}

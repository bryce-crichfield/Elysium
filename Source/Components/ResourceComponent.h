#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    struct ResourceComponent {
        std::string resourceType;
        float amount;
        float maxAmount;
        float gatherRate;

        ResourceComponent(const std::string& type = "Minerals", float amt = 1000.0f)
            : resourceType(type), amount(amt), maxAmount(amt), gatherRate(10.0f) {}

        static constexpr const char* Name() { return "Resource"; }
        static constexpr const char* XmlTag() { return "ResourceComponent"; }

        static void LoadXml(ResourceComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(ResourceComponent& c, Entity e);
        static void BindLua(sol::usertype<ResourceComponent>& ut);
        static void SetFromLua(ResourceComponent& c, sol::object v);
    };
}

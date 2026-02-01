#pragma once
#include "Core/Component.h"
#include <string>
#include <unordered_map>

namespace Elysium {
    struct StorageComponent {
        std::unordered_map<std::string, float> resources;
        bool canAcceptDeposits;

        StorageComponent() : canAcceptDeposits(true) {}

        static constexpr const char* Name() { return "Storage"; }
        static constexpr const char* XmlTag() { return "StorageComponent"; }

        static void LoadXml(StorageComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(StorageComponent& c, Entity e);
        static void BindLua(sol::usertype<StorageComponent>& ut);
        static void SetFromLua(StorageComponent& c, sol::object v);
    };
}

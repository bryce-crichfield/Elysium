#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    struct ScriptComponent {
        std::string scriptName;
        bool isActive = true;
        bool isInitialized = false;

        ScriptComponent() = default;
        ScriptComponent(const std::string& name) : scriptName(name) {}

        static constexpr const char* Name() { return "Script"; }
        static constexpr const char* XmlTag() { return "ScriptComponent"; }

        static void LoadXml(ScriptComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(ScriptComponent& c, Entity e);
        static void BindLua(sol::usertype<ScriptComponent>& ut);
        static void SetFromLua(ScriptComponent& c, sol::object v);
    };
}

#pragma once
#include "Core/Component.h"
#include <string>
#include <vector>

namespace Elysium {
    struct ScriptComponent {
        std::vector<std::string> scriptNames;
        std::vector<bool> isInitialized;
        bool isActive = true;

        ScriptComponent() = default;
        ScriptComponent(const std::string& name) { AddScript(name); }

        void AddScript(const std::string& name) {
            scriptNames.push_back(name);
            isInitialized.push_back(false);
        }

        void RemoveScript(size_t index) {
            if (index < scriptNames.size()) {
                scriptNames.erase(scriptNames.begin() + index);
                isInitialized.erase(isInitialized.begin() + index);
            }
        }

        static constexpr const char* Name() { return "Script"; }
        static constexpr InspectorOrder Order = InspectorOrder::Scripting;
        static constexpr const char* XmlTag() { return "ScriptComponent"; }

        static void LoadXml(ScriptComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const ScriptComponent& c, XMLBuilder& builder);
        static void Inspect(ScriptComponent& c, Entity e, ServiceLocator& services);
        static void BindLua(sol::usertype<ScriptComponent>& ut);
        static void SetFromLua(ScriptComponent& c, sol::object v);
    };
}

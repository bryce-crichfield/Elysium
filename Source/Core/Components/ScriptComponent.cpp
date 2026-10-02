#include "Core/Components/ScriptComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Script.h"
#include "Interfaces/IAssetService.h"
#include "Services/AssetService.h"
#include "Interfaces/IScriptService.h"
#include "Services/ScriptService.h"

namespace Elysium {
    void ScriptComponent::SaveXml(const ScriptComponent& c, XMLBuilder& builder) {
        auto el = builder.AddElement("ScriptComponent");
        for (const auto& name : c.scriptNames) {
            if (!name.empty()) {
                el.AddElement("Script").SetAttribute("name", name.c_str());
            }
        }
    }

    void ScriptComponent::LoadXml(ScriptComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        auto& assetService = services.Get<Elysium::Services::IAssetService>();

        auto loadScript = [&](const char* name) {
            c.AddScript(name);
            assetService.LoadAsset<Script>(Path(name));
        };

        // Backward compat: single scriptName attribute
        const char* scriptName = el->Attribute("scriptName");
        if (scriptName && scriptName[0] != '\0') {
            loadScript(scriptName);
        }

        // New format: child <Script name="..." /> elements
        for (auto* child = el->FirstChildElement("Script"); child; child = child->NextSiblingElement("Script")) {
            const char* name = child->Attribute("name");
            if (name && name[0] != '\0') {
                loadScript(name);
            }
        }
    }

    void ScriptComponent::BindLua(sol::usertype<ScriptComponent>& ut) {
        ut["scriptNames"] = &ScriptComponent::scriptNames;
        ut["isActive"] = &ScriptComponent::isActive;
    }

    void ScriptComponent::SetFromLua(ScriptComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            // Check if it's a list of script names
            sol::object first = t[1];
            if (first.valid() && first.is<std::string>()) {
                c.scriptNames.clear();
                c.isInitialized.clear();
                for (auto& kv : t) {
                    if (kv.second.is<std::string>()) {
                        c.AddScript(kv.second.as<std::string>());
                    }
                }
            } else {
                // Table with named fields
                sol::optional<sol::table> names = t["scriptNames"];
                if (names) {
                    c.scriptNames.clear();
                    c.isInitialized.clear();
                    for (auto& kv : *names) {
                        if (kv.second.is<std::string>()) {
                            c.AddScript(kv.second.as<std::string>());
                        }
                    }
                }
                c.isActive = t.get_or("isActive", c.isActive);
            }
        } else if (v.is<std::string>()) {
            c.scriptNames.clear();
            c.isInitialized.clear();
            c.AddScript(v.as<std::string>());
        }
    }

    REGISTER_COMPONENT(ScriptComponent);
}

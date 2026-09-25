#include "Components/ScriptComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "Core/Script.h"
#include "Interfaces/IAssetService.h"
#include "Services/AssetService.h"
#include "Interfaces/IScriptService.h"
#include "Services/ScriptService.h"
#include "imgui.h"

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

    void ScriptComponent::Inspect(ScriptComponent& c, Entity e, ServiceLocator& services) {

        auto& assetService = services.Get<Elysium::Services::IAssetService>();
        const auto& allAssets = assetService.GetAllAssets();

        std::vector<std::string> scriptPaths;
        scriptPaths.push_back("<None>");
        for (const auto& [name, asset] : allAssets) {
            if (assetService.GetData<Script>(asset.get()) && asset->IsLoaded()) {
                scriptPaths.push_back(name.GetRelativePath());
            }
        }

        PropertyLabel("Is Active");
        std::string activeId = "##ScriptActive_" + std::to_string(e);
        ImGui::Checkbox(activeId.c_str(), &c.isActive);

        SectionHeader("Scripts");

        int removeIndex = -1;
        for (size_t i = 0; i < c.scriptNames.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));

            std::string currentScript = c.scriptNames[i].empty() ? "<None>" : c.scriptNames[i];
            int currentIndex = 0;
            for (size_t j = 0; j < scriptPaths.size(); ++j) {
                if (scriptPaths[j] == currentScript) {
                    currentIndex = static_cast<int>(j);
                    break;
                }
            }

            ImGui::SetNextItemWidth(-(ButtonWidth(ICON_FA_XMARK) + ImGui::GetStyle().ItemSpacing.x));
            std::string comboId = "##Script_" + std::to_string(e) + "_" + std::to_string(i);
            if (ImGui::BeginCombo(comboId.c_str(), currentScript.c_str())) {
                for (size_t j = 0; j < scriptPaths.size(); ++j) {
                    bool isSelected = (currentIndex == static_cast<int>(j));
                    if (ImGui::Selectable(scriptPaths[j].c_str(), isSelected)) {
                        c.scriptNames[i] = (j == 0) ? "" : scriptPaths[j];
                        c.isInitialized[i] = false;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (IconButton(ICON_FA_XMARK, "Remove script")) {
                removeIndex = static_cast<int>(i);
            }

            ImGui::PopID();
        }

        if (removeIndex >= 0) {
            c.RemoveScript(static_cast<size_t>(removeIndex));
        }

        if (ImGui::Button(ICON_FA_PLUS "  Add Script", ImVec2(-FLT_MIN, 0))) {
            c.AddScript("");
        }

        // Show script data for all active scripts
        if (c.isActive) {
            auto& scriptService = services.Get<Elysium::Services::IScriptService>();
            for (size_t i = 0; i < c.scriptNames.size(); ++i) {
                if (!c.scriptNames[i].empty()) {
                    std::string header = ICON_FA_CODE "  " + c.scriptNames[i];
                    if (ImGui::CollapsingHeader(header.c_str())) {
                        scriptService.InspectEntityScript(e, Path(c.scriptNames[i]));
                    }
                }
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

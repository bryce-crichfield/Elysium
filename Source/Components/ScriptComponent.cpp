#include "Components/ScriptComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Application.h"
#include "Services/AssetService.h"
#include "Services/ScriptService.h"
#include "imgui.h"

namespace Elysium {
    void ScriptComponent::LoadXml(ScriptComponent& c, tinyxml2::XMLElement* el) {
        const char* scriptName = el->Attribute("scriptName");
        if (scriptName) {
            c.scriptName = scriptName;
        }
    }

    void ScriptComponent::Inspect(ScriptComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        // Script asset picker
        Label("Script Asset: ");
        auto& assetService = Elysium::Application::GetInstance().GetService<Elysium::Services::AssetService>();
        const auto& allAssets = assetService.GetAllAssets();

        std::vector<std::string> scriptAssetNames;
        scriptAssetNames.push_back("<None>");

        for (const auto& [name, asset] : allAssets) {
            if (asset.GetType() == AssetType::SCRIPT && asset.IsLoaded()) {
                scriptAssetNames.push_back(name);
            }
        }

        std::string currentScript = c.scriptName.empty() ? "<None>" : c.scriptName;
        int currentIndex = 0;
        for (size_t i = 0; i < scriptAssetNames.size(); ++i) {
            if (scriptAssetNames[i] == currentScript) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        std::string comboId = "##ScriptAsset_" + std::to_string(e);
        if (ImGui::BeginCombo(comboId.c_str(), currentScript.c_str())) {
            for (size_t i = 0; i < scriptAssetNames.size(); ++i) {
                bool isSelected = (currentIndex == static_cast<int>(i));
                if (ImGui::Selectable(scriptAssetNames[i].c_str(), isSelected)) {
                    c.scriptName = (i == 0) ? "" : scriptAssetNames[i] + ".lua";
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        Label("Is Active: ");
        std::string activeId = "##ScriptActive_" + std::to_string(e);
        ImGui::Checkbox(activeId.c_str(), &c.isActive);

        if (!c.scriptName.empty() && c.isActive) {
            if (ImGui::CollapsingHeader("Script Data")) {
                auto& scriptService = Elysium::Application::GetInstance().GetService<Elysium::Services::ScriptService>();
                scriptService.InspectEntityScript(e);
            }
        }
    }

    void ScriptComponent::BindLua(sol::usertype<ScriptComponent>& ut) {
        ut["scriptName"] = &ScriptComponent::scriptName;
        ut["isActive"] = &ScriptComponent::isActive;
        ut["isInitialized"] = &ScriptComponent::isInitialized;
    }

    void ScriptComponent::SetFromLua(ScriptComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.scriptName = t.get_or("scriptName", c.scriptName);
            c.isActive = t.get_or("isActive", c.isActive);
        } else if (v.is<std::string>()) {
            c.scriptName = v.as<std::string>();
        }
    }

    REGISTER_COMPONENT(ScriptComponent);
}

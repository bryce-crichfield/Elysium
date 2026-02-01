#include "ScriptEditor.h"
#include "Core/Application.h"
#include "Services/ScriptService.h"
#include "Services/AssetService.h"
#include "Core/Path.h"
#include "imgui.h"
#include "raylib.h"

namespace Elysium {

ScriptEditor::ScriptEditor() : Editor("Script Editor") {
    memset(scriptBuffer, 0, sizeof(scriptBuffer));
}

void ScriptEditor::Initialize() {
    Path hermitCodeFontPath("Fonts/Hermit-Regular.otf");
    font_ = ImGui::GetIO().Fonts->AddFontFromFileTTF(hermitCodeFontPath.GetFullPath().c_str(), (float)fontSize_);
}

void ScriptEditor::Draw(Application& app) {
    if (ImGui::Begin("Script Editor", &isVisible_)) {
        
        auto& assetService = app.GetService<Services::AssetService>();
        const auto& allAssets = assetService.GetAllAssets();

        if (ImGui::BeginCombo("Select Script", selectedAssetName.c_str())) {
            for (const auto& [name, asset] : allAssets) {
                if (asset.GetType() == AssetType::SCRIPT) {
                    bool isSelected = (selectedAssetName == name);
                    if (ImGui::Selectable(name.c_str(), isSelected)) {
                        selectedAssetName = name;
                        // Load script content
                        std::string content = assetService.GetScript(name);
                        strncpy(scriptBuffer, content.c_str(), sizeof(scriptBuffer) - 1);
                        statusMessage = "Loaded script: " + name;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }
        
        if (ImGui::Button("Save")) {
            if (!selectedAssetName.empty()) {
                Asset* asset = assetService.GetAsset(selectedAssetName);
                if (asset) {
                    std::string fullPath = asset->GetPath();
                    if (SaveFileText(fullPath.c_str(), scriptBuffer)) {
                        // Reload in AssetService
                        assetService.ReloadAsset(selectedAssetName);
                        
                        // Reload in ScriptService (using relative path as identifier)
                        auto& scriptService = app.GetService<Services::ScriptService>();
                        scriptService.ReloadScript(asset->GetAssetPath().GetRelativePath());
                        
                        statusMessage = "Saved and Reloaded " + selectedAssetName;
                    } else {
                        statusMessage = "Failed to save " + selectedAssetName;
                    }
                }
            } else {
                statusMessage = "No script selected to save";
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Run")) {
            auto& scriptService = app.GetService<Services::ScriptService>();
            scriptService.ExecuteString(scriptBuffer);
            statusMessage = "Executed script";
        }
        
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Font Size", &fontSize_)) {
            if (fontSize_ < 8) fontSize_ = 8;
            if (fontSize_ > 128) fontSize_ = 128;
            app.RequestFontReload();
        }

        if (!statusMessage.empty()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", statusMessage.c_str());
        }

        ImGui::PushFont(static_cast<ImFont*>(font_));   
        ImGui::InputTextMultiline("##source", scriptBuffer, sizeof(scriptBuffer), ImVec2(-1.0f, -1.0f), ImGuiInputTextFlags_AllowTabInput);
        ImGui::PopFont();
    }
    ImGui::End();
}

}

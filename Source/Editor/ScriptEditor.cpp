#include "ScriptEditor.h"
#include "Core/Application.h"
#include "Core/Asset.h"
#include "Core/Script.h"
#include "Services/ScriptService.h"
#include "Services/AssetService.h"
#include "Core/Path.h"
#include "imgui.h"
#include "raylib.h"

namespace Elysium {

ScriptEditor::ScriptEditor() : Editor("Scripts") {
}

void ScriptEditor::Initialize(const ApplicationConfig& config) {
    const std::string editorFontName = config.editorFontName;
    Path editorFontPath("Fonts/" + editorFontName);
    font_ = ImGui::GetIO().Fonts->AddFontFromFileTTF(editorFontPath.GetFullPath().c_str(), (float)fontSize_);

    textEditor_.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());

}

void ScriptEditor::Draw(Application& app) {
    if (ImGui::Begin("Script Editor", nullptr, ImGuiWindowFlags_NoCollapse)) {
        
        auto& assetService = app.GetService<Services::AssetService>();
        const auto& allAssets = assetService.GetAllAssets();

        if (ImGui::BeginCombo("Select Script", selectedAssetName.c_str())) {
            for (const auto& [name, asset] : allAssets) {
                if (asset->GetType() == AssetType::SCRIPT) {
                    std::string nameStr = name.GetRelativePath();
                    bool isSelected = (selectedAssetName == nameStr);
                    if (ImGui::Selectable(nameStr.c_str(), isSelected)) {
                        selectedAssetName = nameStr;
                        // Load script content
                        Script script = assetService.GetScript(name);
                        textEditor_.SetText(script.source);
                        statusMessage = "Loaded script: " + nameStr;
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
                IAsset* asset = assetService.GetAsset(Path(selectedAssetName));
                if (asset) {
                    std::string fullPath = asset->GetPath().GetFullPath();
                    auto scriptSource = textEditor_.GetText();
                    if (SaveFileText(fullPath.c_str(), scriptSource.c_str())) {
                        // Reload in AssetService
                        assetService.ReloadAsset(asset->GetType(), asset->GetPath());

                        // Reload in ScriptService (using relative path as identifier)
                        auto& scriptService = app.GetService<Services::ScriptService>();
                        scriptService.ReloadScript(asset->GetPath());
                        
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
            std::string scriptSource = textEditor_.GetText();
            scriptService.ExecuteString(scriptSource);
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
        textEditor_.Render("Script Editor Text Area", ImVec2(-1.0f, -1.0f), false);
        ImGui::PopFont();
    }
    ImGui::End();
}

}

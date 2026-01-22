#include "ScriptEditor.h"
#include "Core/Application.h"
#include "Services/ScriptService.h"
#include "Utilities/Path.h"
#include "imgui.h"
#include "raylib.h"

namespace Elysium {

ScriptEditor::ScriptEditor() : Editor("Script Editor") {
    memset(scriptBuffer, 0, sizeof(scriptBuffer));
}

void ScriptEditor::Draw(Application& app) {
    if (ImGui::Begin("Script Editor", &isVisible_)) {
        
        ImGui::InputText("Path (Relative to Assets/)", scriptPath, sizeof(scriptPath));
        
        if (ImGui::Button("Load")) {
            Path path(scriptPath);
            char* text = LoadFileText(path.c_str());
            if (text) {
                strncpy(scriptBuffer, text, sizeof(scriptBuffer) - 1);
                UnloadFileText(text);
                statusMessage = "Loaded " + std::string(path.c_str());
            } else {
                statusMessage = "Failed to load " + std::string(path.c_str());
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            Path path(scriptPath);
            if (SaveFileText(path.c_str(), scriptBuffer)) {
                statusMessage = "Saved " + std::string(path.c_str());
            } else {
                statusMessage = "Failed to save " + std::string(path.c_str());
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Run")) {
            auto& scriptService = app.GetService<Services::ScriptService>();
            scriptService.ExecuteString(scriptBuffer);
            statusMessage = "Executed script";
        }
        
        if (!statusMessage.empty()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", statusMessage.c_str());
        }

        ImGui::InputTextMultiline("##source", scriptBuffer, sizeof(scriptBuffer), ImVec2(-1.0f, -1.0f), ImGuiInputTextFlags_AllowTabInput);
    }
    ImGui::End();
}

}

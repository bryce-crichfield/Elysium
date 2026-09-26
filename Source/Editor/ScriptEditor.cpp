#include "ScriptEditor.h"
#include "Core/Application.h"
#include "Core/Asset.h"
#include "Core/Path.h"
#include "Core/Script.h"
#include "Editor/Widgets.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IScriptService.h"
#include <algorithm>
#include <fstream>

namespace Elysium {

namespace {
constexpr int kMinFontSize = 8;
constexpr int kMaxFontSize = 128;
}  // namespace

ScriptEditor::ScriptEditor(ServiceLocator& services) : Editor(services, Title) {
}

void ScriptEditor::Initialize(const ApplicationConfig& config) {
    Path editorFontPath(Theme().EditorFont, PathRoot::Engine);
    font_ = ImGui::GetIO().Fonts->AddFontFromFileTTF(editorFontPath.GetFullPath().c_str(), (float)fontSize_);

    textEditor_.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
}

void ScriptEditor::Draw() {
    if (BeginWindow()) {
        DrawToolbar();

        if (!statusMessage_.empty()) {
            ColoredText(statusIsError_ ? Palette().Error : Palette().TextMuted, statusMessage_.c_str());
        }

        // With no script picked the buffer is a scratchpad for Run.
        ImGui::PushFont(static_cast<ImFont*>(font_));
        textEditor_.Render("ScriptText", ImVec2(-1.0f, -1.0f), false);
        ImGui::PopFont();
    }
    EndWindow();
}

void ScriptEditor::DrawToolbar() {
    auto& assetService = services_.Get<Services::IAssetService>();

    // Script picker fills whatever the buttons and font size leave.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float fontWidth = ImGui::GetFrameHeight() * 4.0f;
    const float trailing = ButtonWidth(ICON_FA_FLOPPY_DISK "  Save") + ButtonWidth(ICON_FA_PLAY "  Run") + fontWidth +
                           style.ItemSpacing.x * 3.0f;
    ImGui::SetNextItemWidth(-trailing);
    const char* preview = selectedAssetName_.empty() ? "Select a script" : selectedAssetName_.c_str();
    if (ImGui::BeginCombo("##Script", preview)) {
        for (const auto& [path, asset] : assetService.GetAllAssets()) {
            if (!assetService.GetData<Script>(asset.get())) continue;
            const std::string name = path.GetRelativePath();
            const bool isSelected = selectedAssetName_ == name;
            if (ImGui::Selectable(name.c_str(), isSelected)) SelectScript(name);
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(selectedAssetName_.empty());
    if (PrimaryButton(ICON_FA_FLOPPY_DISK "  Save")) SaveScript();
    ImGui::EndDisabled();
    ItemTooltip("Write to disk and hot-reload");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLAY "  Run")) {
        services_.Get<Services::IScriptService>().ExecuteString(textEditor_.GetText());
        SetStatus("Executed script");
    }
    ItemTooltip("Execute the buffer as a Lua chunk");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(fontWidth);
    if (ImGui::InputInt("##FontSize", &fontSize_)) {
        fontSize_ = std::clamp(fontSize_, kMinFontSize, kMaxFontSize);
        services_.Get<Services::IApplicationService>().RequestFontReload();
    }
    ItemTooltip("Editor font size");
}

void ScriptEditor::SelectScript(const std::string& name) {
    selectedAssetName_ = name;
    if (auto* script = services_.Get<Services::IAssetService>().Get<Script>(Path(name))) {
        textEditor_.SetText(script->source);
        SetStatus("Loaded " + name);
    }
}

void ScriptEditor::SaveScript() {
    auto& assetService = services_.Get<Services::IAssetService>();
    IAsset* asset = assetService.GetAsset(Path(selectedAssetName_));
    if (!asset) {
        SetStatus("Script is no longer loaded: " + selectedAssetName_, true);
        return;
    }

    std::ofstream file(asset->GetPath().GetFullPath(), std::ios::binary | std::ios::trunc);
    file << textEditor_.GetText();
    const bool saved = file.good();
    file.close();
    if (!saved) {
        SetStatus("Failed to save " + selectedAssetName_, true);
        return;
    }

    // Capture the path before reloading — ReloadAsset destroys the existing instance
    // `asset` points to.
    Path scriptPath = asset->GetPath();
    assetService.ReloadAsset(asset);
    services_.Get<Services::IScriptService>().ReloadScript(scriptPath);
    SetStatus("Saved and reloaded " + selectedAssetName_);
}

void ScriptEditor::SetStatus(const std::string& message, bool isError) {
    statusMessage_ = message;
    statusIsError_ = isError;
}

}

#include <algorithm>
#include <cstdint>  // TextEditor.h uses uint8_t/uint64_t without including it (GCC 16 no longer does transitively)
#include <fstream>
#include <sstream>

#include "TextEditor.h"
#include "Core/Asset.h"
#include "Core/Path.h"
#include "Editor/ContentPane.h"
#include "Editor/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IEditorService.h"
#include "Interfaces/IScriptService.h"

namespace Elysium {

namespace {

constexpr float kMinFontSize = 8.0f;
constexpr float kMaxFontSize = 64.0f;

// Scripts (Lua) and shaders (GLSL): the file's text in a code editor, saved straight back
// to disk and hot-reloaded if the engine has it loaded.
class CodePane : public ContentPane {
   public:
    CodePane(ServiceLocator& services, const Services::EditorDocument& document)
        : services_(services), fullPath_(document.fullPath), isScript_(document.kind == AssetKind::Script) {
        editor_.SetLanguageDefinition(isScript_ ? TextEditor::LanguageDefinition::Lua() : TextEditor::LanguageDefinition::GLSL());
        std::ifstream file(fullPath_, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        editor_.SetText(text.str());
        if (!file) SetStatus("Couldn't read " + fullPath_, true);
    }

    void DrawToolbar() override {
        if (isScript_) {
            ImGui::SameLine();
            if (IconButton(ICON_FA_PLAY, "Run the buffer as a Lua chunk")) {
                services_.Get<Services::IScriptService>().ExecuteString(editor_.GetText());
                SetStatus("Ran the buffer");
            }
        }
        if (!status_.empty()) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ColoredText(statusIsError_ ? Editor::Palette().Error : Editor::Palette().TextMuted, status_.c_str());
        }

        // Right side: font size.
        const float width = ImGui::GetFrameHeight() * 4.0f;
        AlignRight(width);
        ImGui::SetNextItemWidth(width);
        if (ImGui::InputFloat("##FontSize", &fontSize_, 1.0f, 0.0f, "%.0f")) fontSize_ = std::clamp(fontSize_, kMinFontSize, kMaxFontSize);
        ItemTooltip("Font size");
    }

    void Draw() override {
        ImGui::PushFont(EditorStyle::CodeFont(), fontSize_);
        editor_.Render("Code", ImVec2(-1.0f, -1.0f), false);
        ImGui::PopFont();
    }

    bool CanSave() const override { return true; }

    bool SaveAs(const std::string& fullPath) override { return Write(fullPath); }

    bool Save() override {
        if (!Write(fullPath_)) return false;

        // Hot-reload what the engine has loaded from this file.
        auto& assets = services_.Get<Services::IAssetService>();
        const Path path = Path::FromFullPath(fullPath_);
        if (IAsset* asset = assets.GetAsset(path)) {
            assets.ReloadAsset(asset);
            if (isScript_) services_.Get<Services::IScriptService>().ReloadScript(path);
        }
        SetStatus("Saved");
        return true;
    }

   private:
    bool Write(const std::string& fullPath) {
        std::ofstream file(fullPath, std::ios::binary | std::ios::trunc);
        file << editor_.GetText();
        if (!file.good()) SetStatus("Failed to save", true);
        return file.good();
    }

    void SetStatus(const std::string& message, bool isError = false) {
        status_ = message;
        statusIsError_ = isError;
    }

    ServiceLocator& services_;
    std::string fullPath_;
    bool isScript_;
    TextEditor editor_;
    std::string status_;
    bool statusIsError_ = false;
    float fontSize_ = 18.0f;
};

}  // namespace

std::unique_ptr<ContentPane> MakeCodePane(ServiceLocator& services, const Services::EditorDocument& document) {
    return std::make_unique<CodePane>(services, document);
}

}  // namespace Elysium

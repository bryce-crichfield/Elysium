#include "Editor/Widgets/AssetFileDialog.h"

#include <filesystem>

#include "Core/Path.h"
#include "Editor/Style/AssetStyle.h"
#include "Editor/Widgets/Widgets.h"

namespace Elysium {

namespace {

constexpr const char* kPopupId = "###AssetFileDialog";

// Kinds File > New can make: the rest (sounds, textures) come from outside the editor.
constexpr AssetKind kCreatableKinds[] = {AssetKind::Scene, AssetKind::Prefab, AssetKind::Script, AssetKind::Sprite,
                                         AssetKind::Shader};

const AssetKindFolder& FolderOf(AssetKind kind) {
    for (const auto& folder : AssetKindFolders()) {
        if (folder.kind == kind) return folder;
    }
    return AssetKindFolders().front();
}

}  // namespace

void AssetFileDialog::SetKind(AssetKind kind) {
    kind_ = kind;
    extension_ = FolderOf(kind).extensions.front();
    if (!folderFixed_) folder_ = FolderOf(kind).folder;
}

void AssetFileDialog::OpenNew(std::optional<AssetKind> kind, const std::string& folder) {
    mode_ = Mode::New;
    kindFixed_ = kind.has_value();
    folderFixed_ = !folder.empty();
    folder_ = folder;
    SetKind(kind.value_or(AssetKind::Prefab));
    snprintf(name_, sizeof(name_), "New%s", StyleOf(kind_).label);
    pendingOpen_ = true;
}

void AssetFileDialog::OpenSaveAs(AssetKind kind, const std::string& fullPath) {
    const std::filesystem::path relative(Path::FromFullPath(fullPath).GetRelativePath());
    mode_ = Mode::SaveAs;
    kindFixed_ = true;
    folderFixed_ = true;
    folder_ = relative.parent_path().generic_string();
    kind_ = kind;
    extension_ = relative.extension().string();
    snprintf(name_, sizeof(name_), "%sCopy", relative.stem().string().c_str());
    pendingOpen_ = true;
}

std::optional<AssetFileDialog::Result> AssetFileDialog::Draw() {
    if (pendingOpen_) {
        ImGui::OpenPopup(kPopupId);
        pendingOpen_ = false;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(Editor::Theme().DialogWidth, 0.0f), ImGuiCond_Appearing);
    const std::string title = std::string(mode_ == Mode::New ? "New Asset" : "Save As") + kPopupId;
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings)) return std::nullopt;

    // Kind: a chip per creatable kind to pick from, or just the fixed one.
    PropertyLabel("Type");
    if (kindFixed_) {
        KindChip(kind_, true, StyleOf(kind_).label);
    } else {
        // Chips flow left to right from the label column, wrapping back to it when a row fills.
        const float rowStart = ImGui::GetCursorPosX();
        const float rowEnd = ImGui::GetWindowContentRegionMax().x;
        const ImGuiStyle& style = ImGui::GetStyle();
        bool first = true;
        for (AssetKind kind : kCreatableKinds) {
            const AssetStyle kindStyle = StyleOf(kind);
            const float width = ImGui::CalcTextSize((std::string(kindStyle.icon) + "  " + kindStyle.label).c_str()).x + style.FramePadding.x * 2.0f;
            if (!first) {
                ImGui::SameLine();
                if (ImGui::GetCursorPosX() + width > rowEnd) {
                    ImGui::NewLine();
                    ImGui::SetCursorPosX(rowStart);
                }
            }
            if (KindChip(kind, kind == kind_, kindStyle.label)) SetKind(kind);
            first = false;
        }
    }

    PropertyLabel("Name");
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool submitted = ImGui::InputText("##name", name_, sizeof(name_), ImGuiInputTextFlags_EnterReturnsTrue);

    // Validate: a plain file name that doesn't clash with an existing file.
    const std::string name = name_;
    const std::string relativePath = folder_ + "/" + name + extension_;
    const std::string fullPath = Path(relativePath).GetFullPath();
    const char* problem = nullptr;
    if (name.empty()) problem = "Enter a name";
    else if (name.find_first_of("/\\:*?\"<>|.") != std::string::npos) problem = "Name can't contain path characters";
    else if (std::filesystem::exists(fullPath)) problem = "A file with that name already exists";

    ImGui::Spacing();
    if (problem) {
        ColoredText(Editor::Palette().Error, problem);
    } else {
        const AssetStyle style = StyleOf(kind_);
        ColoredText(style.color, style.icon);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", relativePath.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    const char* confirmLabel = mode_ == Mode::New ? "Create" : "Save";
    AlignRight(ButtonWidth(confirmLabel) + ButtonWidth("Cancel") + ImGui::GetStyle().ItemSpacing.x);
    ImGui::BeginDisabled(problem != nullptr);
    const bool confirm = PrimaryButton(confirmLabel) || (submitted && !problem);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);

    std::optional<Result> result;
    if (confirm && !problem) result = Result{mode_, kind_, fullPath};
    if (confirm || cancel) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return result;
}

}  // namespace Elysium

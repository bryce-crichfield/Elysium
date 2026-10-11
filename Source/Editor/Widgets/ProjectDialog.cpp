#include "Editor/Widgets/ProjectDialog.h"

#include <filesystem>

#include "Core/Path.h"
#include "Core/Project.h"
#include "Editor/Widgets/Widgets.h"

namespace Elysium {

namespace {

constexpr const char* kPopupId = "Open Project";
constexpr const char* kBrowseLabel = ICON_FA_FOLDER_OPEN "  Browse...";

std::string CurrentProjectFile() { return Path::GetAssetsRoot() + "Project.xml"; }

}  // namespace

void ProjectDialog::Open() {
    snprintf(path_, sizeof(path_), "%s", CurrentProjectFile().c_str());
    failedPath_.clear();
    pendingOpen_ = true;
}

std::optional<std::string> ProjectDialog::Draw() {
    if (pendingOpen_) {
        ImGui::OpenPopup(kPopupId);
        pendingOpen_ = false;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(Editor::Theme().DialogWidth, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(kPopupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) return std::nullopt;

    if (std::optional<std::string> picked = browser_.Poll()) {
        snprintf(path_, sizeof(path_), "%s", picked->c_str());
    }

    PropertyLabel("Path");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ButtonWidth(kBrowseLabel) - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool submitted = ImGui::InputText("##path", path_, sizeof(path_), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::BeginDisabled(browser_.IsOpen());
    if (ImGui::Button(kBrowseLabel)) {
        const std::filesystem::path start = std::filesystem::path(path_).parent_path();
        browser_.Open("Open Project", start.string(), {"Project", "Project.xml"});
    }
    ImGui::EndDisabled();

    const std::string path = path_;
    const std::string current = std::filesystem::weakly_canonical(CurrentProjectFile()).generic_string();
    const char* problem = nullptr;
    if (path.empty()) problem = "Enter the path to a Project.xml";
    else if (!std::filesystem::is_regular_file(path)) problem = "No file at that path";
    else if (std::filesystem::weakly_canonical(path).generic_string() == current) problem = "That project is already open";
    else if (path == failedPath_) problem = "Not a valid Project.xml (see the log)";

    if (problem) {
        ImGui::Spacing();
        ColoredText(Editor::Palette().Error, problem);
    }

    ImGui::Spacing();
    ImGui::Separator();
    AlignRight(ButtonWidth("Open") + ButtonWidth("Cancel") + ImGui::GetStyle().ItemSpacing.x);
    ImGui::BeginDisabled(problem != nullptr || browser_.IsOpen());
    const bool confirm = PrimaryButton("Open") || (submitted && !problem);
    ImGui::EndDisabled();
    ImGui::SameLine();
    // Escape belongs to the OS browser while it's up.
    const bool cancel = ImGui::Button("Cancel") || (!browser_.IsOpen() && ImGui::IsKeyPressed(ImGuiKey_Escape));

    // Loaded once on confirm, not every frame (it logs): the app restarts on this file, so it
    // has to be a project main can open.
    std::optional<std::string> result;
    ProjectConfig config;
    if (confirm && !problem) {
        if (ProjectConfig::FromXML(path, config)) result = path;
        else failedPath_ = path;
    }
    if (result || cancel) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return result;
}

}  // namespace Elysium

#include "Editor/EditorUI.h"

#include "Editor/AssetEditor.h"
#include "Editor/HierarchyEditor.h"
#include "Editor/InspectorEditor.h"
#include "Editor/LogEditor.h"
#include "Editor/NetworkEditor.h"
#include "Editor/Style/Theme.h"
#include "Editor/ViewportEditor.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IEditorService.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"

namespace Elysium {

EditorUI::EditorUI(ServiceLocator& services) : services_(services) {
    // Registration order is tab order within each dock node.
    editors_.push_back(std::make_unique<HierarchyEditor>(services_));
    editors_.push_back(std::make_unique<InspectorEditor>(services_));
    editors_.push_back(std::make_unique<ViewportEditor>(services_));
    editors_.push_back(std::make_unique<AssetEditor>(services_));
    editors_.push_back(std::make_unique<LogEditor>(services_));
    editors_.push_back(std::make_unique<NetworkEditor>(services_));
}

void EditorUI::Initialize(const ApplicationConfig& config) {
    config_ = config;

    rlImGuiSetup(true);
    EditorStyle::LoadTheme("Dark");
    EditorStyle::LoadFonts();

    // Must be set before the first ImGui::NewFrame() (rlImGuiBegin() in Draw), or ImGui
    // asserts — docking state only matters once editor windows exist (Editor mode),
    // but the flag itself is harmless to leave enabled in Play mode.
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().ConfigDockingAlwaysTabBar = true;

    for (auto& editor : editors_) {
        editor->Initialize(config_);
    }
}

void EditorUI::Shutdown() {
    rlImGuiShutdown();
}

void EditorUI::OnModeChanged(AppMode mode) {
    for (auto& editor : editors_) {
        editor->SetVisible(mode == AppMode::Editor && editor->IsDocked());
    }
    if (mode != AppMode::Editor) editorLayoutBuilt_ = false;
}

void EditorUI::ReloadFonts() {
    ImGui::GetIO().Fonts->Clear();
    rlImGuiBeginInitImGui();
    rlImGuiEndInitImGui();
    EditorStyle::LoadFonts();
    for (auto& editor : editors_) {
        editor->Initialize(config_);
    }
}

void EditorUI::DrawMenuBar(AppMode mode) {
    if (!ImGui::BeginMainMenuBar()) return;

    // File acts on the asset open in the Viewport.
    if (ImGui::BeginMenu("File")) {
        ViewportEditor* viewport = GetEditor<ViewportEditor>();
        const bool hasDocument = services_.Get<Services::IEditorService>().GetActiveDocumentInfo() != nullptr;
        if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS "  New...", "Ctrl+N", false, viewport)) viewport->BeginNew();
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save", "Ctrl+S", false, viewport && hasDocument)) viewport->SaveActive();
        if (ImGui::MenuItem("       Save As...", "Ctrl+Shift+S", false, viewport && hasDocument)) viewport->BeginSaveAs();
        ImGui::EndMenu();
    }

    // Edit acts on the active tab's command history, so each open scene undoes its own work.
    auto& editor = services_.Get<Services::IEditorService>();
    if (ImGui::BeginMenu("Edit")) {
        Elysium::CommandHistory* history = editor.GetHistory();
        const char* undoing = history ? history->UndoLabel() : nullptr;
        const char* redoing = history ? history->RedoLabel() : nullptr;

        // Naming what will happen ("Undo Delete Entity") is the difference between trusting
        // the shortcut and guessing at it.
        const std::string undoLabel = std::string(ICON_FA_ROTATE_LEFT "  Undo") + (undoing ? std::string(" ") + undoing : "");
        const std::string redoLabel = std::string(ICON_FA_ROTATE_RIGHT "  Redo") + (redoing ? std::string(" ") + redoing : "");

        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, undoing != nullptr)) editor.Undo();
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Shift+Z", false, redoing != nullptr)) editor.Redo();

        ImGui::Separator();
        const bool hasSelection = !editor.GetSelectedEntities().empty();
        if (ImGui::MenuItem(ICON_FA_SCISSORS "  Cut", "Ctrl+X", false, hasSelection)) editor.CutSelection();
        if (ImGui::MenuItem(ICON_FA_COPY "  Copy", "Ctrl+C", false, hasSelection)) editor.CopySelection();
        // From the menu there is no cursor to paste under, so it lands where the editor
        // camera is pointed, which is the part of the scene being looked at.
        if (ImGui::MenuItem(ICON_FA_PASTE "  Paste", "Ctrl+V", false, editor.CanPaste()))
            editor.Paste(editor.GetEditorCamera().position);
        ImGui::EndMenu();
    }

    // Wherever focus is, except inside a text field — Ctrl+Z belongs to the box being typed
    // in. Ctrl+Y is the second redo binding out of habit; both reach the same history.
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) editor.Undo();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) editor.Redo();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)) editor.Redo();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_X)) editor.CutSelection();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C)) editor.CopySelection();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V)) editor.Paste(editor.GetEditorCamera().position);
    }

    if (ImGui::BeginMenu("View")) {
        // Docked panels are fixed; only the dialogs are opened from here.
        for (auto& panel : editors_) {
            if (panel->IsDocked()) continue;
            if (ImGui::MenuItem(panel->GetName().c_str())) panel->SetVisible(true);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Theme")) {
            // Re-read on selection, so edits to the file show up without a restart.
            for (const std::string& name : EditorStyle::AvailableThemes()) {
                if (ImGui::MenuItem(name.c_str(), nullptr, name == EditorStyle::CurrentTheme().name) &&
                    EditorStyle::LoadTheme(name)) {
                    pendingFontReload_ = true;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Mode")) {
        auto& app = services_.Get<Services::IApplicationService>();
        if (ImGui::MenuItem("Editor", "F1", mode == AppMode::Editor)) app.SetMode(AppMode::Editor);
        if (ImGui::MenuItem("Play", "F2", mode == AppMode::Play)) app.SetMode(AppMode::Play);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void EditorUI::BuildDockLayout() {
    // Full-window dockspace
    ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

    // Rebuilt whenever the viewport resizes (e.g. the window maximizing after the first
    // frame): otherwise the central node soaks up all the growth and the side columns
    // stay at their first-frame width.
    const ImVec2 workSize = ImGui::GetMainViewport()->WorkSize;
    if (editorLayoutBuilt_ && workSize.x == editorLayoutWidth_ && workSize.y == editorLayoutHeight_) return;
    editorLayoutBuilt_ = true;
    editorLayoutWidth_ = workSize.x;
    editorLayoutHeight_ = workSize.y;
    focusDefaultTabs_ = true;

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, workSize);

    // Hierarchy left, Inspector right, Console/Assets along the bottom of the
    // middle, the Viewport (every open asset, one tab each) in the center. The layout is fixed: panels
    // can't be closed, moved or undocked (see Editor::BeginWindow).
    ImGuiID dockLeft, dockRemain;
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.18f, &dockLeft, &dockRemain);
    ImGuiID dockRight, dockMiddle;
    ImGui::DockBuilderSplitNode(dockRemain, ImGuiDir_Right, 0.26f, &dockRight, &dockMiddle);
    ImGuiID dockBottom, dockCenter;
    ImGui::DockBuilderSplitNode(dockMiddle, ImGuiDir_Down, 0.28f, &dockBottom, &dockCenter);

    ImGui::DockBuilderDockWindow(HierarchyEditor::Title, dockLeft);
    ImGui::DockBuilderDockWindow(ViewportEditor::Title, dockCenter);
    ImGui::DockBuilderDockWindow(AssetEditor::Title, dockBottom);
    ImGui::DockBuilderDockWindow(LogEditor::Title, dockBottom);
    ImGui::DockBuilderDockWindow(InspectorEditor::Title, dockRight);

    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorUI::Draw(AppMode mode) {
    if (pendingFontReload_) {
        ReloadFonts();
        pendingFontReload_ = false;
    }

    rlImGuiBegin();

    if (mode == AppMode::Editor) {
        DrawMenuBar(mode);
        BuildDockLayout();
    }

    for (auto& editor : editors_) {
        if (editor->IsVisible()) editor->Draw();
    }

    // Tabs open on Hierarchy, Inspector, Viewport and Assets; the panels have to exist first.
    if (focusDefaultTabs_ && mode == AppMode::Editor) {
        focusDefaultTabs_ = false;
        for (const char* title : {HierarchyEditor::Title, InspectorEditor::Title, AssetEditor::Title, ViewportEditor::Title}) {
            ImGui::SetWindowFocus(title);
        }
    }

    rlImGuiEnd();
}

}  // namespace Elysium

#pragma once

#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "Core/Entity.h"
#include "Core/ServiceLocator.h"

namespace Elysium {

struct ApplicationConfig;
class EditorApplication;

namespace EditorStyle {
struct Palette;
struct Theme;
}  // namespace EditorStyle

class Editor {
   public:
    // The active editor look, loaded from a theme file (Editor/Theme.h). Include
    // Editor/Theme.h to use the result; shared widgets that use them are in Editor/Widgets.h.
    static const EditorStyle::Palette& Palette();
    static const EditorStyle::Theme& Theme();

    Editor(EditorApplication& editor, const std::string& name);
    virtual ~Editor() = default;

    virtual void Initialize(const ApplicationConfig& config) {}
    virtual void Draw() = 0;

    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
    const std::string& GetName() const { return name_; }

    // Docked panels are part of the fixed editor layout: always shown, never closed, moved
    // or undocked. Others are hidden until opened from the View menu.
    virtual bool IsDocked() const { return true; }

   protected:
    // Opens this docked panel's window, titled by its name. Always pair with EndWindow,
    // whatever this returns. `showTitle` false hides the dock tab, for a panel alone in its
    // node whose own content says what it is.
    bool BeginWindow(ImGuiWindowFlags flags = 0, bool showTitle = true) {
        ImGuiWindowClass locked;
        locked.DockNodeFlagsOverrideSet = (int)ImGuiDockNodeFlags_NoUndocking | (int)ImGuiDockNodeFlags_NoDockingSplit |
                                          (int)ImGuiDockNodeFlags_NoWindowMenuButton | (int)ImGuiDockNodeFlags_NoCloseButton |
                                          (showTitle ? 0 : (int)ImGuiDockNodeFlags_NoTabBar);
        ImGui::SetNextWindowClass(&locked);
        return ImGui::Begin(name_.c_str(), nullptr, flags | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
    }
    void EndWindow() { ImGui::End(); }

    EditorApplication& editor_;
    ServiceLocator& services_;  // the engine's, from editor_
    std::string name_;
    bool isVisible_ = false;
};

}  // namespace Elysium

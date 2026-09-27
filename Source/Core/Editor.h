#pragma once

#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "Core/Entity.h"
#include "Core/ServiceLocator.h"

namespace Elysium {

struct ApplicationConfig;

namespace EditorStyle {
struct Palette;
struct Theme;
}  // namespace EditorStyle

// Where a component's section sits in the Inspector, top to bottom: what the entity is,
// where it is, what it looks like, how it behaves. Ties sort by name. A component opts in
// with `static constexpr InspectorOrder Order = ...;`; without one it sorts last.
enum class InspectorOrder : int {
    Identity,   // Name
    Transform,
    Hierarchy,  // Parent
    Geometry,   // shapes, sprites, text, tiles, bounds
    Layer,
    Material,
    Shader,
    Rendering,  // camera, UI
    Physics,    // collision and motion
    Gameplay,
    Scripting,
    Other,
};

template<typename T>
constexpr InspectorOrder InspectorOrderOf() {
    if constexpr (requires { { T::Order } -> std::convertible_to<InspectorOrder>; }) return T::Order;
    else return InspectorOrder::Other;
}

template<typename T>
concept Inspectable = requires(T& c, Entity e, ServiceLocator& services) {
    { T::Inspect(c, e, services) } -> std::same_as<void>;
};

class Editor {
   public:
    // The active editor look, loaded from a theme file (Editor/Theme.h). Include
    // Editor/Theme.h to use the result; shared widgets that use them are in Editor/Widgets.h.
    static const EditorStyle::Palette& Palette();
    static const EditorStyle::Theme& Theme();

    Editor(ServiceLocator& services, const std::string& name) : services_(services), name_(name) {}
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

    ServiceLocator& services_;
    std::string name_;
    bool isVisible_ = false;
};

}  // namespace Elysium

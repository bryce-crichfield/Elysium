#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Core/Entity.h"
#include "Core/Math/MathTypes.h"
#include "Core/Reflection.h"

namespace Elysium {

class OverlayPainter;
class World;

class EditorApplication;

// The viewport mouse, in the spaces tools actually work in.
struct ViewportInput {
    Vector2 mouseWorld;
    // Framebuffer space, which is what picking works in.
    Vector2 mouseFb;
    float worldPerPixel = 1.0f;
    bool hovered = false;
    bool clicked = false;
    bool rightClicked = false;
    // Where the viewport image sits on screen, for a tool drawing its own screen-space chrome.
    Rectangle imageScreenRect;
};

// What the Viewport lends a tool for one frame. ViewportEditor owns the camera, the
// framebuffer and the picking machinery; a tool only reads them through here, which is what
// keeps a tool from needing to know about RenderSystem or the editor camera at all.
struct ToolContext {
    World& world;
    EditorApplication& editor;
    ViewportInput input;

    // Entities under the cursor, smallest-first, with locked layers already dropped. Not free
    // (it queries the render system), so call it on a click rather than every frame.
    std::function<std::vector<Entity>()> pick;
    // Entities whose rendered bounds overlap a world-space rectangle, for box select.
    std::function<std::vector<Entity>(Rectangle)> pickRect;
    std::function<Vector2(Vector2)> worldToScreen;
};

// Which manipulator a tool puts on the selection, if any.
//
// This used to be a mode on the Viewport with its own three toolbar buttons, usable only while
// the select tool was active -- so "what the mouse does" was split across a tool and a separate
// mode, and the buttons were dead weight under every other tool. Move, Rotate and Scale are now
// tools in their own right: each picks entities like Select and puts its own gizmo on them.
enum class GizmoMode { None, Move, Rotate, Scale };

// How a tool's one-line status reads. The toolbar owns the colors, so every tool's hint looks
// the same wherever it came from.
enum class ToolStatusLevel {
    Hint,     // what to do next, or what is missing before the tool can act
    Working,  // the tool is armed and will act on the next click
    Warning,  // something will refuse the action
};

// The line a tool contributes to the toolbar beside the tool buttons.
struct ToolStatus {
    std::string text;
    ToolStatusLevel level = ToolStatusLevel::Hint;
};

// A tool's settings: the fields, plus the object they live on. Empty `object` means no settings.
struct ToolParameters {
    void* object = nullptr;
    FieldList fields;

    bool Empty() const { return object == nullptr || fields.empty(); }
};

// One viewport mode. Exactly one tool is active at a time and it owns the left mouse button,
// which is the whole point: before this, painting and navmesh editing were independent
// toggles that each had to remember to switch the other one off.
class ViewportTool {
   public:
    virtual ~ViewportTool() = default;

    virtual const char* Name() const = 0;
    virtual const char* Icon() const = 0;
    virtual const char* Tooltip() const = 0;

    // Why this tool can't be used right now, or null when it can. Drives the disabled state of
    // its toolbar button and its tooltip, and a tool that becomes unusable while active is
    // switched away from. One mechanism for every reason a tool might not apply, rather than a
    // scattering of special cases.
    virtual const char* Unavailable(EditorApplication& editor, bool isScene) const {
        (void)editor;
        (void)isScene;
        return nullptr;
    }

    // The manipulator this tool puts on the selection. None for tools that own the mouse
    // themselves -- a paint stroke that also dragged the last selection around would be unusable.
    virtual GizmoMode Gizmo() const { return GizmoMode::None; }
    // Whether a click in empty space means "pick an entity" for this tool. True for the select
    // family, which is also what gets the right-click pick menu; other tools use both buttons
    // themselves. Separate from Gizmo() because the plain Select tool picks without a gizmo.
    virtual bool PicksEntities() const { return false; }

    virtual void OnActivate(EditorApplication& editor) { (void)editor; }
    virtual void OnDeactivate(EditorApplication& editor) { (void)editor; }

    // This tool's settings, drawn by the Tool Panel. Reuses component reflection rather than
    // inventing a third parameter system: a FieldInfo already carries the label, the type, and
    // the widget hints (Range/Speed/Asset), so the panel picks the widget from the type and no
    // tool writes any ImGui for its own settings.
    //
    // `object` comes from inside the override, where `this` is the concrete tool, so the field
    // address lambdas cast back to the type they were formed on.
    virtual ToolParameters Parameters() { return {}; }

    // One line beside the tool buttons saying what the tool will do next, or what is stopping it.
    // Empty for nothing to say.
    //
    // This replaced a DrawToolbar that let a tool emit arbitrary ImGui, which had become a second
    // channel competing with Parameters(): the paint tool wrote its own colored status text and the
    // navmesh tool grew a whole brush picker there, so "a tool declares its settings and writes no
    // UI" was not actually true. Settings go in Parameters(), a status line comes back from here,
    // and no tool touches ImGui.
    virtual ToolStatus Status(EditorApplication& editor) const {
        (void)editor;
        return {};
    }

    virtual void DrawOverlay(ToolContext& context, OverlayPainter& painter) {
        (void)context;
        (void)painter;
    }

    // True when the tool has taken the mouse this frame, so the viewport neither picks nor pans.
    virtual bool HandleInput(ToolContext& context) = 0;
};

}  // namespace Elysium

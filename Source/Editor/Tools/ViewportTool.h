#pragma once

#include <functional>
#include <vector>
#include "Core/Entity.h"
#include "Core/MathTypes.h"

namespace Elysium {

class OverlayPainter;
class World;

namespace Services {
class IEditorService;
}
namespace Systems {
class NavMeshSystem;
}

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
    Services::IEditorService& editor;
    ViewportInput input;

    // Entities under the cursor, smallest-first, with locked layers already dropped. Not free
    // (it queries the render system), so call it on a click rather than every frame.
    std::function<std::vector<Entity>()> pick;
    // Entities whose rendered bounds overlap a world-space rectangle, for box select.
    std::function<std::vector<Entity>(Rectangle)> pickRect;
    std::function<Vector2(Vector2)> worldToScreen;

    // The active scene's navmesh bake, when it has one.
    const Systems::NavMeshSystem* nav = nullptr;
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
    virtual const char* Unavailable(Services::IEditorService& editor, bool isScene) const {
        (void)editor;
        (void)isScene;
        return nullptr;
    }

    // Whether the transform gizmo is live while this tool is active. Only the select tool wants
    // it; a paint stroke that also dragged the last selection around would be unusable.
    virtual bool UsesGizmo() const { return false; }
    // Whether this tool draws nav areas itself, so the generic spatial overlay doesn't
    // double-draw them underneath.
    virtual bool OwnsNavAreaOverlay() const { return false; }

    virtual void OnActivate(Services::IEditorService& editor) { (void)editor; }
    virtual void OnDeactivate(Services::IEditorService& editor) { (void)editor; }

    // Extra controls beside the tool buttons, drawn only while this tool is active.
    virtual void DrawToolbar(Services::IEditorService& editor) { (void)editor; }
    virtual void DrawOverlay(ToolContext& context, OverlayPainter& painter) {
        (void)context;
        (void)painter;
    }

    // True when the tool has taken the mouse this frame, so the viewport neither picks nor pans.
    virtual bool HandleInput(ToolContext& context) = 0;
};

}  // namespace Elysium

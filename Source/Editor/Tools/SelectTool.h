#pragma once

#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include "Editor/Tools/ViewportTool.h"

namespace Elysium {

// The select family: pick entities, drag a box to pick several, and -- for the three that carry a
// gizmo -- manipulate what is picked.
//
// One class instantiated four times rather than four classes, because picking is the whole tool
// and the gizmo is the only difference. Move/Rotate/Scale were three buttons on the toolbar that
// only did anything under Select; as tools they sit beside it, so "what does the mouse do" is
// answered in one place instead of by a tool plus a separate mode.
//
// Selection happens on mouse *release*, not on press, because a press is ambiguous -- it might be
// the start of a box drag. Clicking the same spot repeatedly cycles through overlapping entities,
// which is how you reach something behind a floor tile.
class SelectTool : public ViewportTool {
   public:
    // `gizmo` None is the plain select tool, which picks without a manipulator in the way.
    explicit SelectTool(GizmoMode gizmo) : gizmo_(gizmo) {}

    const char* Name() const override;
    const char* Icon() const override;
    const char* Tooltip() const override;

    GizmoMode Gizmo() const override { return gizmo_; }
    bool PicksEntities() const override { return true; }

    ToolStatus Status(Services::IEditorService& editor) const override;

    void OnDeactivate(Services::IEditorService& editor) override;

    void DrawOverlay(ToolContext& context, OverlayPainter& painter) override;
    bool HandleInput(ToolContext& context) override;

   private:
    void PickAtCursor(ToolContext& context);
    void SelectInBox(ToolContext& context);

    GizmoMode gizmo_;

    // Box drag state. `pressed_` spans press to release; `boxing_` turns on once the cursor has
    // travelled far enough that this is clearly a drag and not a click.
    bool pressed_ = false;
    bool boxing_ = false;
    Vector2 pressWorld_{};
    Vector2 pressFb_{};

    // Click cycling: repeat-clicking one spot advances through what is stacked there.
    Vector2 lastClickFb_{-1.0f, -1.0f};
    size_t lastClickIndex_ = 0;
};

}  // namespace Elysium

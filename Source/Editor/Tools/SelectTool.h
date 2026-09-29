#pragma once

#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include "Editor/Tools/ViewportTool.h"

namespace Elysium {

// The default tool: pick entities, and drag a box to pick several.
//
// Selection happens on mouse *release*, not on press, because a press is ambiguous — it might
// be the start of a box drag. Clicking the same spot repeatedly cycles through overlapping
// entities, which is how you reach something behind a floor tile.
class SelectTool : public ViewportTool {
   public:
    const char* Name() const override { return "Select"; }
    const char* Icon() const override;
    const char* Tooltip() const override { return "Select (1) - click to pick, drag to box-select"; }

    bool UsesGizmo() const override { return true; }

    void OnDeactivate(Services::IEditorService& editor) override;

    void DrawOverlay(ToolContext& context, OverlayPainter& painter) override;
    bool HandleInput(ToolContext& context) override;

   private:
    void PickAtCursor(ToolContext& context);
    void SelectInBox(ToolContext& context);

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

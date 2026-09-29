#pragma once

#include "Editor/PrefabPainter.h"
#include "Editor/Tools/ViewportTool.h"

namespace Elysium {

class LayerDrawer;

// Stamps prefab placements onto the active layer. The stroke logic lives in PrefabPainter; this
// is the tool shell around it, plus the brush it reads from the layer drawer (the brush is a
// property of what you are painting with, so it belongs with the picker that chooses it).
//
// A whole drag is one undo step: the tool opens a gesture on the first cell and closes it when
// the button comes up.
class PaintTool : public ViewportTool {
   public:
    explicit PaintTool(const LayerDrawer& drawer) : drawer_(drawer) {}

    const char* Name() const override { return "Paint"; }
    const char* Icon() const override;
    const char* Tooltip() const override { return "Paint prefabs (2) - click to place, drag to fill, Alt or right-click erases"; }

    const char* Unavailable(Services::IEditorService& editor, bool isScene) const override;

    void OnDeactivate(Services::IEditorService& editor) override;

    void DrawOverlay(ToolContext& context, OverlayPainter& painter) override;
    bool HandleInput(ToolContext& context) override;

   private:
    const LayerDrawer& drawer_;
    PrefabPainter painter_;
    // Whether a stroke's undo gesture is currently open.
    bool stroking_ = false;
};

}  // namespace Elysium

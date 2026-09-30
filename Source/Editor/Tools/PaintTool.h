#pragma once

#include <string>
#include "Editor/PrefabPainter.h"
#include "Editor/Tools/ViewportTool.h"

namespace Elysium {

// Stamps prefab placements onto the active layer. The stroke logic lives in PrefabPainter; this
// is the tool shell around it, plus the brush, which is the tool's own setting: it used to live
// in the layer drawer because that panel had room for it, which meant the paint mode and its
// brush were owned by two different things.
//
// A whole drag is one undo step: the tool opens a gesture on the first cell and closes it when
// the button comes up.
class PaintTool : public ViewportTool {
   public:
    const char* Name() const override { return "Paint"; }
    const char* Icon() const override;
    const char* Tooltip() const override { return "Paint prefabs (6) - click to place, drag to fill, Alt or right-click erases"; }

    const char* Unavailable(Services::IEditorService& editor, bool isScene) const override;

    ToolParameters Parameters() override {
        return {this, {Field("Prefab", &PaintTool::brushPrefab_, "prefab").Asset(AssetKind::Prefab)}};
    }

    void OnDeactivate(Services::IEditorService& editor) override;
    ToolStatus Status(Services::IEditorService& editor) const override;

    void DrawOverlay(ToolContext& context, OverlayPainter& painter) override;
    bool HandleInput(ToolContext& context) override;

   private:
    // Project-relative path of the prefab each click stamps; empty until one is picked.
    std::string brushPrefab_;
    PrefabPainter painter_;
    // Whether a stroke's undo gesture is currently open.
    bool stroking_ = false;
};

}  // namespace Elysium

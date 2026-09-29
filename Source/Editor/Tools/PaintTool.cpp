#include "Editor/Tools/PaintTool.h"

#include "Editor/LayerDrawer.h"
#include "extras/IconsFontAwesome6.h"
#include "Interfaces/IEditorService.h"
#include "imgui.h"

namespace Elysium {

const char* PaintTool::Icon() const { return ICON_FA_PAINT_ROLLER; }

const char* PaintTool::Unavailable(Services::IEditorService& editor, bool isScene) const {
    // Each reason gets its own message: a disabled button that doesn't say what is missing is
    // just a dead button.
    if (!isScene) return "Painting needs a scene's layers";
    if (drawer_.BrushPrefab().empty()) return "Pick a prefab to paint in the layer drawer";

    const std::string& layer = editor.GetActiveLayer();
    if (layer.empty()) return "Focus a layer to paint onto";
    if (editor.IsLayerLocked(layer)) return "The focused layer is locked";
    return nullptr;
}

void PaintTool::OnDeactivate(Services::IEditorService& editor) {
    painter_.EndStroke();
    if (stroking_) {
        editor.EndGesture();
        stroking_ = false;
    }
}

bool PaintTool::HandleInput(ToolContext& context) {
    const bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // One gesture per stroke, so dragging across forty cells is one Ctrl+Z rather than forty.
    // Opened on the press and closed on release, independently of whether any cell was actually
    // painted -- an empty gesture leaves no entry behind.
    if (down && context.input.hovered && !stroking_) {
        context.editor.BeginGesture("Paint Prefabs");
        stroking_ = true;
    } else if (!down && stroking_) {
        context.editor.EndGesture();
        stroking_ = false;
    }

    return painter_.HandleInput(context.world, context.editor, context.input, drawer_.BrushPrefab());
}

void PaintTool::DrawOverlay(ToolContext& context, OverlayPainter& painter) {
    painter_.DrawOverlay(context.editor, context.input, painter);
}

}  // namespace Elysium

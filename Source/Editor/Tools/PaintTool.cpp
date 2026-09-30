#include "Editor/Tools/PaintTool.h"

#include <string>

#include "extras/IconsFontAwesome6.h"
#include "Interfaces/IEditorService.h"
#include "imgui.h"

namespace Elysium {

const char* PaintTool::Icon() const { return ICON_FA_PAINT_ROLLER; }

const char* PaintTool::Unavailable(Services::IEditorService& editor, bool isScene) const {
    (void)editor;
    // Only what you cannot fix from inside the tool belongs here, because Unavailable both
    // disables the button and steps away from an active tool. The brush lives in this tool's own
    // settings panel, which only shows while the tool is active -- gating on it made the tool
    // impossible to select and its brush impossible to set. A missing brush or an unfocused
    // layer leaves the tool selectable but inert, and says so in the toolbar instead.
    if (!isScene) return "Painting needs a scene's layers";
    return nullptr;
}

void PaintTool::OnDeactivate(Services::IEditorService& editor) {
    painter_.EndStroke();
    if (stroking_) {
        editor.EndGesture();
        stroking_ = false;
    }
}

ToolStatus PaintTool::Status(Services::IEditorService& editor) const {
    // Everything that stops a stroke landing is said here rather than by disabling the tool, so the
    // tool stays selectable while you go and fix it. Most specific thing first.
    const std::string& layer = editor.GetActiveLayer();

    if (brushPrefab_.empty()) return {"Pick a prefab in the tool settings " ICON_FA_WRENCH};
    if (layer.empty()) return {"Focus a layer in the layer drawer to paint onto"};
    if (editor.IsLayerLocked(layer)) return {layer + " is locked", ToolStatusLevel::Warning};
    if (!editor.GetGrid().snapEnabled) {
        return {"Painting onto " + layer + " (snap off - freehand)", ToolStatusLevel::Working};
    }
    return {"Painting onto " + layer, ToolStatusLevel::Working};
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

    return painter_.HandleInput(context.world, context.editor, context.input, brushPrefab_);
}

void PaintTool::DrawOverlay(ToolContext& context, OverlayPainter& painter) {
    painter_.DrawOverlay(context.editor, context.input, painter);
}

}  // namespace Elysium

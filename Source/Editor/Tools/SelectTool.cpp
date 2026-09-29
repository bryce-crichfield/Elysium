#include "Editor/Tools/SelectTool.h"

#include <algorithm>
#include "Core/Editor.h"
#include "Editor/OverlayPainter.h"
#include "Editor/Theme.h"
#include "extras/IconsFontAwesome6.h"
#include "Interfaces/IEditorService.h"
#include "imgui.h"

namespace Elysium {

namespace {

// How far the cursor must travel from the press before a click becomes a box drag. Small enough
// that a deliberate drag is recognised immediately, large enough that a shaky click isn't.
constexpr float kBoxDragPixels = 4.0f;

// Ctrl or Shift extends the selection instead of replacing it.
bool Additive() {
    const ImGuiIO& io = ImGui::GetIO();
    return io.KeyCtrl || io.KeyShift;
}

Rectangle RectBetween(Vector2 a, Vector2 b) {
    const float x = std::min(a.x, b.x), y = std::min(a.y, b.y);
    return Rectangle{x, y, std::fabs(a.x - b.x), std::fabs(a.y - b.y)};
}

}  // namespace

const char* SelectTool::Icon() const { return ICON_FA_ARROW_POINTER; }

void SelectTool::OnDeactivate(Services::IEditorService&) {
    pressed_ = false;
    boxing_ = false;
}

bool SelectTool::HandleInput(ToolContext& context) {
    const ViewportInput& in = context.input;

    if (pressed_) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (!boxing_ && (in.mouseFb - pressFb_).Length() > kBoxDragPixels) boxing_ = true;
            // Only a box drag owns the mouse. A plain press must not, or the camera couldn't be
            // panned by dragging and the gizmo would never see the button.
            return boxing_;
        }

        pressed_ = false;
        if (boxing_) {
            boxing_ = false;
            SelectInBox(context);
        } else {
            PickAtCursor(context);
        }
        return true;
    }

    if (!in.hovered) return false;
    if (in.clicked) {
        pressed_ = true;
        boxing_ = false;
        pressWorld_ = in.mouseWorld;
        pressFb_ = in.mouseFb;
    }
    return false;
}

void SelectTool::PickAtCursor(ToolContext& context) {
    const std::vector<Entity> hits = context.pick();

    const bool samePos = !hits.empty() && (pressFb_ - lastClickFb_).Length() < Editor::Theme().ClickCycleDistance;
    const size_t index = samePos ? (lastClickIndex_ + 1) % hits.size() : 0;
    lastClickFb_ = pressFb_;
    lastClickIndex_ = index;

    if (!hits.empty()) {
        context.editor.SelectEntity(hits[index], Additive());
    } else if (!Additive()) {
        // Clicking empty space clears, unless you are extending a selection, where it would be
        // an easy way to throw the whole thing away by accident.
        context.editor.ClearSelection();
    }
}

void SelectTool::SelectInBox(ToolContext& context) {
    const std::vector<Entity> inside = context.pickRect(RectBetween(pressWorld_, context.input.mouseWorld));

    if (!Additive()) context.editor.ClearSelection();
    for (Entity entity : inside) {
        if (!context.editor.IsSelected(entity)) context.editor.SelectEntity(entity, true);
    }
}

void SelectTool::DrawOverlay(ToolContext& context, OverlayPainter& painter) {
    if (!boxing_) return;

    const Rectangle box = RectBetween(pressWorld_, context.input.mouseWorld);
    painter.Rect({box.x, box.y}, {box.x + box.width, box.y + box.height}, Editor::Palette().Selection, 0.12f,
                 painter.LineWidth());
}

}  // namespace Elysium

#pragma once

#include "Core/MathTypes.h"
#include "Editor/ViewportPanel.h"
#include "extras/IconsFontAwesome6.h"

namespace Elysium {

class ViewportTool;

// The active tool's settings, over the left edge of the viewport image. Chrome comes from
// ViewportPanel; this supplies only the body.
//
// It draws nothing itself. A tool declares its settings as a typed FieldList via
// ViewportTool::Parameters(), and the panel renders that list with InspectFields, which picks each
// widget from the field's type. So a tool gains a setting by naming a member, not by writing UI,
// and no tool's settings live in some other panel that happens to have room -- which is where the
// paint brush used to be.
class ToolPanel {
public:
    bool IsOpen() const { return panel_.IsOpen(); }
    void SetOpen(bool open) { panel_.SetOpen(open); }

    void DrawToolbarButton(const char* unavailable = nullptr) { panel_.DrawToolbarButton(unavailable); }

    // Draws the panel for `tool`. No-op while closed or without an active tool.
    void Draw(ViewportTool* tool, Rectangle imageScreenRect);

private:
    ViewportPanel panel_{PanelEdge::Left, 240.0f, ICON_FA_WRENCH, "Tool settings"};
};

}  // namespace Elysium

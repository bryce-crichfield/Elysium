#include "Editor/ToolPanel.h"

#include <string>

#include "Core/Reflection.h"
#include "Editor/Tools/ViewportTool.h"
#include "Editor/Widgets.h"

namespace Elysium {

void ToolPanel::Draw(ViewportTool* tool, Rectangle imageScreenRect) {
    if (!tool) return;

    // Headed by the tool, so it is obvious the panel follows the active tool rather than being a
    // fixed set of settings.
    const std::string title = std::string(tool->Icon()) + "  " + tool->Name();
    panel_.Draw(imageScreenRect, title, [tool] {
        ToolParameters parameters = tool->Parameters();
        if (parameters.Empty()) MutedText("This tool has no settings");
        else InspectFields(parameters.object, parameters.fields);
    });
}

}  // namespace Elysium

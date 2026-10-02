#include "Editor/Viewport/ViewportPanel.h"

#include <algorithm>

#include "Editor/Editor.h"
#include "Editor/Style/Theme.h"
#include "Editor/Widgets/Widgets.h"
#include "imgui.h"

namespace Elysium {

using EditorStyle::Palette;

void ViewportPanel::DrawToolbarButton(const char* unavailable) {
    // Closing it here rather than at the call site: a panel that cannot apply to this tab has no
    // business staying open across the switch, and one rule beats each caller remembering.
    if (unavailable) open_ = false;
    if (GatedToggleIconButton(icon_, open_, tooltip_, unavailable)) open_ = !open_;
}

void ViewportPanel::Draw(Rectangle imageScreenRect, const std::string& title,
                         const std::function<void()>& body) {
    if (!open_) return;

    const float width = std::min(width_, imageScreenRect.width);
    // A child window takes its position from the cursor, not SetNextWindowPos, so park the cursor
    // at the edge the panel hangs off to anchor it there.
    const float x = edge_ == PanelEdge::Left ? imageScreenRect.x
                                            : imageScreenRect.x + imageScreenRect.width - width;
    ImGui::SetCursorScreenPos(ImVec2(x, imageScreenRect.y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Palette::WithAlpha(Editor::Palette().Base, 0.94f));
    const std::string id = std::string("##ViewportPanel") + icon_;
    if (ImGui::BeginChild(id.c_str(), ImVec2(width, imageScreenRect.height), ImGuiChildFlags_Borders)) {
        SectionHeader(title.c_str());
        body();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace Elysium

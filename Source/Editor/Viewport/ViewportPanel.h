#pragma once

#include <functional>
#include <string>
#include "Core/Math/MathTypes.h"

namespace Elysium {

// Which edge of the viewport image a panel hangs off. Two panels on opposite edges can be open
// at once; two on the same edge would overlap, so the edge is part of a panel's identity.
enum class PanelEdge { Left, Right };

// The chrome shared by every panel that floats over the viewport image: the toolbar toggle that
// opens it, the anchoring to an edge, the translucent background, and the header.
//
// This exists because the layer drawer and the tool panel were two classes with the same shape --
// the same open flag, the same toggle button, the same SetCursorScreenPos/PushStyleColor/BeginChild
// dance, even the same copied comment -- differing only in edge, width, icon and body. You had to
// read both to notice they were one concept, and their open/close rules had drifted apart. Now a
// panel is this plus a body.
class ViewportPanel {
public:
    ViewportPanel(PanelEdge edge, float width, const char* icon, const char* tooltip)
        : edge_(edge), width_(width), icon_(icon), tooltip_(tooltip) {}

    bool IsOpen() const { return open_; }
    void SetOpen(bool open) { open_ = open; }

    // The toolbar toggle. Always drawn: while `unavailable` is non-null it is greyed and says
    // why, rather than disappearing, so the toolbar keeps the same shape on every kind of tab.
    // An unavailable panel is also closed, since its contents no longer apply.
    void DrawToolbarButton(const char* unavailable = nullptr);

    // Draws the panel over `imageScreenRect`, with `title` as its header and `body` as its
    // contents. No-op while closed.
    void Draw(Rectangle imageScreenRect, const std::string& title, const std::function<void()>& body);

private:
    PanelEdge edge_;
    float width_;
    const char* icon_;
    const char* tooltip_;
    bool open_ = false;
};

}  // namespace Elysium

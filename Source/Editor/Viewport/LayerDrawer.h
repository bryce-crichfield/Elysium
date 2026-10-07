#pragma once

#include <string>
#include "Core/Math/MathTypes.h"
#include "Editor/Viewport/ViewportPanel.h"
#include "extras/IconsFontAwesome6.h"

namespace Elysium {
class Scene;
class EditorApplication;

// The Viewport's layer drawer: a panel that slides out over the right edge of a scene tab,
// listing the scene's layers top-down by z. Selecting one makes it active, which filters the
// Hierarchy and is where painted prefabs land. Each row toggles lock (its entities can't be
// picked, dragged or deleted), solo (only soloed layers draw) and hidden.
//
// All of that state lives in EditorApplication and is session-only, so none of it reaches the
// scene file. The paint brush used to live here too; it is a tool setting, so it moved to the
// tool panel with the rest of the paint tool's settings.
//
// The panel chrome (toggle button, anchoring, background, header) comes from ViewportPanel; this
// supplies only the list.
class LayerDrawer {
public:
    bool IsOpen() const { return panel_.IsOpen(); }
    void SetOpen(bool open) { panel_.SetOpen(open); }

    void DrawToolbarButton(const char* unavailable = nullptr) { panel_.DrawToolbarButton(unavailable); }

    // Draws the drawer over `imageScreenRect`. No-op while closed.
    void Draw(Scene& scene, EditorApplication& editor, Rectangle imageScreenRect);

private:
    void DrawLayerRow(Scene& scene, EditorApplication& editor, const std::string& name, int zIndex);

    ViewportPanel panel_{PanelEdge::Right, 260.0f, ICON_FA_LAYER_GROUP, "Layers"};
};

}  // namespace Elysium

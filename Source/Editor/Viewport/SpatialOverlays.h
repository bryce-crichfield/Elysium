#pragma once
#include "Core/Components/NavAreaComponent.h"
#include "imgui.h"

namespace Elysium {
class World;
class OverlayPainter;
class EditorApplication;

struct SpatialOverlayOptions {
    bool colliders = false;
    bool navAreas = false;
};

const ImVec4& NavAreaColor(NavAreaType type);

// Collider polygons and nav areas for every entity the options
// ask for; the selected entities always show all of theirs plus their anchor. Entities on a
// layer the layer drawer hides are skipped, so hiding a layer hides its overlays with it.
void DrawSpatialOverlays(World& world, EditorApplication& editor, const SpatialOverlayOptions& options, OverlayPainter& painter);

// The overlay toggles, one icon button each, for the viewport footer. Was a toolbar button that
// opened a popup of three checkboxes -- two clicks and a menu to do what is really three switches,
// and it sat among the tool buttons as if it were one.
void DrawOverlayToggles(SpatialOverlayOptions& options);

}  // namespace Elysium

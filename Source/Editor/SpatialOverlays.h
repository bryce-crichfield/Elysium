#pragma once
#include "Components/NavAreaComponent.h"
#include "imgui.h"

namespace Elysium {
class World;
class OverlayPainter;
namespace Services { class IEditorService; }

struct SpatialOverlayOptions {
    bool colliders = false;
    bool occluders = false;
    bool navAreas = false;
};

const ImVec4& NavAreaColor(NavAreaType type);

// Collider polygons, occluder footprints/volumes and nav areas for every entity the options
// ask for; the selected entities always show all of theirs plus their anchor. Entities on a
// layer the layer drawer hides are skipped, so hiding a layer hides its overlays with it.
void DrawSpatialOverlays(World& world, Services::IEditorService& editor, const SpatialOverlayOptions& options, OverlayPainter& painter);

// A toolbar button that opens the overlay checkboxes.
void DrawOverlaysMenu(SpatialOverlayOptions& options);

}  // namespace Elysium

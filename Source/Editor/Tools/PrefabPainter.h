#pragma once

#include <string>
#include <unordered_set>
#include "Core/Entity.h"
#include "Core/Math/MathTypes.h"
#include "Editor/Tools/ViewportTool.h"  // ViewportInput

namespace Elysium {
class World;
class OverlayPainter;
class EditorApplication;

// Paints prefab placements onto the active layer at grid positions — the tool that makes walls
// and floors just prefabs on a layer rather than a tile map.
//
// Click places one; dragging keeps placing as the cursor crosses into new cells. A cell that
// already holds a placement of the same prefab on the same layer is skipped, so dragging back
// over your own work doesn't stack duplicates. Right-click (or Alt-click) erases the placement
// under the cursor on the active layer instead.
class PrefabPainter {
public:
    // Returns true when it consumed the input, so the viewport doesn't also pick or pan.
    bool HandleInput(World& world, EditorApplication& editor, const ViewportInput& input,
                     const std::string& prefabPath);

    // The brush preview: the cell about to be painted, at the snapped position.
    void DrawOverlay(EditorApplication& editor, const ViewportInput& input, OverlayPainter& painter) const;

    // Call when paint mode turns off, so a new stroke doesn't inherit the old one's history.
    void EndStroke() { painted_.clear(); stroking_ = false; }

private:
    // A placement of `prefabPath` already on `layer` within half a cell of `world`.
    Entity PlacementAt(World& world, EditorApplication& editor, Vector2 world_, const std::string& layer,
                       const std::string& prefabPath, Vector2 tolerance) const;

    // Cells painted during the current drag, so one stroke never double-places.
    std::unordered_set<long long> painted_;
    bool stroking_ = false;
};

}  // namespace Elysium

#include "Editor/PrefabPainter.h"

#include <cmath>

#include "Components/LayerComponent.h"
#include "Components/PrefabInstanceComponent.h"
#include "Core/PrefabInstance.h"
#include "Components/TransformComponent.h"
#include "Core/Editor.h"
#include "Core/Geometry.h"
#include "Core/Path.h"
#include "Core/World.h"
#include "Editor/OverlayPainter.h"
#include "Editor/Theme.h"
#include "Interfaces/IEditorService.h"
#include "imgui.h"

namespace Elysium {

using namespace Services;
using EditorStyle::Palette;

namespace {

// A cell key from a snapped world position. Quantised to whole units first so float drift
// across frames can't produce two keys for the same cell.
long long CellKey(Vector2 snapped) {
    const long long x = (long long)std::llround(snapped.x);
    const long long y = (long long)std::llround(snapped.y);
    return (x << 32) ^ (y & 0xFFFFFFFFLL);
}

// The diamond (or square) outline of one cell, centred on `center`.
std::vector<Vector2> CellOutline(Vector2 center, const GridSettings& grid) {
    const float halfW = grid.Cell().x * 0.5f, halfH = grid.Cell().y * 0.5f;
    if (grid.lattice == GridLattice::Square) {
        return {{center.x - halfW, center.y - halfH},
                {center.x + halfW, center.y - halfH},
                {center.x + halfW, center.y + halfH},
                {center.x - halfW, center.y + halfH}};
    }
    return TranslatePolygon(IsoDiamond(grid.Cell().x, grid.Cell().y), center);
}

}  // namespace

Entity PrefabPainter::PlacementAt(World& world, IEditorService& editor, Vector2 at, const std::string& layer,
                                  const std::string& prefabPath, Vector2 tolerance) const {
    // Empty prefabPath matches any placement (erasing); otherwise compare resolved full paths,
    // since a placement stores its src relative to whichever file places it.
    const std::string wanted = prefabPath.empty() ? std::string() : Path(prefabPath).GetFullPath();
    Entity found = INVALID_ENTITY;
    world.Query<TransformComponent, PrefabInstanceComponent>([&](Entity e, auto& transform, auto& instance) {
        if (found != INVALID_ENTITY) return;
        if (!PrefabInstances::IsRoot(world, e)) return;
        if (!wanted.empty() && !SamePath(instance.FullPath(), wanted)) return;
        if (editor.GetEntityLayer(e) != layer) return;
        if (std::fabs(transform.worldX - at.x) > tolerance.x) return;
        if (std::fabs(transform.worldY - at.y) > tolerance.y) return;
        found = e;
    });
    return found;
}

bool PrefabPainter::HandleInput(World& world, IEditorService& editor, const ViewportInput& in,
                               const std::string& prefabPath) {
    if (!in.hovered || prefabPath.empty()) return false;

    const std::string& layer = editor.GetActiveLayer();
    if (layer.empty() || editor.IsLayerLocked(layer)) return false;

    const GridSettings& grid = editor.GetGrid();
    const Vector2 target = editor.SnapToGrid(in.mouseWorld);
    // Without snapping every pixel is its own cell, so the stroke's duplicate suppression has
    // nothing to key on; fall back to a brush-sized tolerance.
    // Neighbouring snap targets are half a cell apart, so a half-cell tolerance reaches them and
    // every click reads as "already painted here" and silently does nothing. Quarter-cell keeps
    // the test inside one cell.
    const Vector2 tolerance = grid.snapEnabled ? Vector2{grid.Cell().x * 0.25f, grid.Cell().y * 0.25f}
                                              : Vector2{1.0f, 1.0f};

    // Erase: right-click, or Alt-click for people who keep the right button for the camera.
    const bool erasing = in.rightClicked || (in.clicked && ImGui::GetIO().KeyAlt);
    if (erasing) {
        // Erase whatever placement is here on this layer, not only ones of the brush's prefab.
        if (Entity hit = PlacementAt(world, editor, target, layer, "", tolerance); hit != INVALID_ENTITY) {
            editor.DeleteEntity(hit);
        }
        return true;
    }

    const bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (!down) {
        EndStroke();
        return false;
    }
    if (!stroking_) {
        stroking_ = true;
        painted_.clear();
    }

    // One placement per cell per stroke, so dragging back over the stroke is harmless.
    const long long key = CellKey(target);
    if (!painted_.insert(key).second) return true;
    // And never stack onto a cell that already holds this prefab from an earlier stroke.
    if (PlacementAt(world, editor, target, layer, prefabPath, tolerance) != INVALID_ENTITY) return true;

    const Entity placed = editor.InstantiatePrefab(Path(prefabPath).GetFullPath());
    if (placed == INVALID_ENTITY || !world.HasComponent<TransformComponent>(placed)) return true;

    // Position is local to the parent, which for a scene document is none.
    Vector2 local = target;
    const Entity parent = world.GetParent(placed);
    if (parent != INVALID_ENTITY && world.HasComponent<TransformComponent>(parent)) {
        const auto& parentTransform = world.GetComponent<TransformComponent>(parent);
        local.x -= parentTransform.worldX;
        local.y -= parentTransform.worldY;
    }
    auto& transform = world.GetComponent<TransformComponent>(placed);
    transform.localX = local.x;
    transform.localY = local.y;
    transform.worldX = target.x;
    transform.worldY = target.y;

    // Paint onto the focused layer, overriding whatever the prefab file declares. This is why
    // LayerComponent is PlacementOwned — otherwise the override couldn't be saved.
    if (world.HasComponent<LayerComponent>(placed)) {
        world.GetComponent<LayerComponent>(placed).name = layer;
    } else {
        world.AddComponent<LayerComponent>(placed, LayerComponent(layer));
    }
    return true;
}

void PrefabPainter::DrawOverlay(IEditorService& editor, const ViewportInput& in, OverlayPainter& painter) const {
    if (!in.hovered) return;
    const std::string& layer = editor.GetActiveLayer();
    if (layer.empty()) return;

    const GridSettings& grid = editor.GetGrid();
    const Vector2 target = editor.SnapToGrid(in.mouseWorld);
    const bool erasing = ImGui::GetIO().KeyAlt;
    const ImVec4& color = erasing ? Editor::Palette().Error : Editor::Palette().Accent;

    painter.Polygon(CellOutline(target, grid), color, 0.22f, painter.LineWidth() + 1.0f);
    painter.Cross(target, color);
}

}  // namespace Elysium

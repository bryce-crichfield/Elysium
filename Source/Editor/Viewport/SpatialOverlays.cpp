#include "Editor/Viewport/SpatialOverlays.h"
#include <unordered_set>
#include "Core/Components/ColliderComponent.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Geometry/Polygon.h"
#include "Core/World.h"
#include "Editor/Viewport/OverlayPainter.h"
#include "Editor/Style/Theme.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IEditorService.h"

namespace Elysium {

using EditorStyle::Palette;

const ImVec4& NavAreaColor(NavAreaType type) {
    switch (type) {
        case NavAreaType::Blocked:  return Editor::Palette().Error;
        case NavAreaType::Cost:     return Editor::Palette().Warning;
        case NavAreaType::Walkable: break;
    }
    return Editor::Palette().Success;
}

void DrawSpatialOverlays(World& world, Services::IEditorService& editor, const SpatialOverlayOptions& options, OverlayPainter& painter) {
    const auto& selectedList = editor.GetSelectedEntities();
    const std::unordered_set<Entity> selected(selectedList.begin(), selectedList.end());
    // A hidden layer hides its overlays too — otherwise hiding a layer still leaves its collider
    // wireframes cluttering the viewport. Selection still wins, so you can inspect
    // something you picked before hiding its layer.
    auto shown = [&](Entity e, bool option) {
        if (selected.contains(e)) return true;
        if (!option) return false;
        return !editor.IsLayerHidden(editor.GetEntityLayer(e));
    };

    world.Query<TransformComponent, NavAreaComponent>([&](Entity e, auto& t, auto& area) {
        if (!shown(e, options.navAreas)) return;
        auto polygon = area.LocalPolygon();
        if (polygon.Empty()) return;
        painter.Polygon(polygon.Translated({t.worldX, t.worldY}).Points(), Palette::WithAlpha(NavAreaColor(area.Type()), 0.7f), 0.14f);
    });

    world.Query<TransformComponent, ColliderComponent>([&](Entity e, auto& t, auto& collider) {
        if (!shown(e, options.colliders)) return;
        painter.Polygon(collider.GetPolygon(t.worldX, t.worldY).Points(), Palette::WithAlpha(Editor::Palette().AccentHover, collider.isTrigger ? 0.45f : 1.0f));
    });

    for (Entity e : selectedList) {
        if (!world.HasComponent<TransformComponent>(e)) continue;
        const auto& t = world.GetComponent<TransformComponent>(e);
        painter.Cross({t.worldX, t.worldY}, Editor::Palette().Selection);
    }
}

void DrawOverlayToggles(SpatialOverlayOptions& options) {
    // Icon-only, like the rest of the footer: each is one switch, and the tooltip carries the name.
    // Colliders get a shape rather than anything sprite-like, since a collider outline is not tied
    // to the sprite it happens to sit under.
    auto toggle = [](const char* icon, bool& value, const char* tooltip) {
        if (ToggleIconButton(icon, value, tooltip)) value = !value;
        ImGui::SameLine();
    };
    toggle(ICON_FA_SHAPES, options.colliders, "Show collider outlines");
    toggle(ICON_FA_ROUTE, options.navAreas, "Show nav areas");
    // The note is prose, not a control, so it sits past a rule like every other group.
    ToolbarSeparator();
    ImGui::AlignTextToFramePadding();
    MutedText("Selected entities always show theirs");
}

}  // namespace Elysium

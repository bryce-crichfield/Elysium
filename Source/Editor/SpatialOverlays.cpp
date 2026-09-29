#include "Editor/SpatialOverlays.h"
#include <unordered_set>
#include "Components/ColliderComponent.h"
#include "Components/OccluderComponent.h"
#include "Components/TransformComponent.h"
#include "Core/Geometry.h"
#include "Core/World.h"
#include "Editor/OverlayPainter.h"
#include "Editor/Theme.h"
#include "Editor/Widgets.h"
#include "Interfaces/IEditorService.h"
#include "Systems/OcclusionSystem.h"

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
    // and occluder wireframes cluttering the viewport. Selection still wins, so you can inspect
    // something you picked before hiding its layer.
    auto shown = [&](Entity e, bool option) {
        if (selected.contains(e)) return true;
        if (!option) return false;
        return !editor.IsLayerHidden(editor.GetEntityLayer(e));
    };

    world.Query<TransformComponent, NavAreaComponent>([&](Entity e, auto& t, auto& area) {
        if (!shown(e, options.navAreas)) return;
        auto polygon = area.LocalPolygon();
        if (polygon.empty()) return;
        painter.Polygon(TranslatePolygon(polygon, {t.worldX, t.worldY}), Palette::WithAlpha(NavAreaColor(area.Type()), 0.7f), 0.14f);
    });

    world.Query<TransformComponent, OccluderComponent>([&](Entity e, auto&, auto& occluder) {
        if (!shown(e, options.occluders)) return;
        auto volume = Systems::ResolveOccluder(world, e);
        const ImVec4& color = occluder.isStatic ? Editor::Palette().Warning : Editor::Palette().Success;
        if (volume.height > 0.0f) painter.Polygon(volume.volume, Palette::WithAlpha(color, 0.35f));
        painter.Polygon(volume.footprint, color);
    });

    world.Query<TransformComponent, ColliderComponent>([&](Entity e, auto& t, auto& collider) {
        if (!shown(e, options.colliders)) return;
        painter.Polygon(collider.GetPolygon(t.worldX, t.worldY), Palette::WithAlpha(Editor::Palette().AccentHover, collider.isTrigger ? 0.45f : 1.0f));
    });

    for (Entity e : selectedList) {
        if (!world.HasComponent<TransformComponent>(e)) continue;
        const auto& t = world.GetComponent<TransformComponent>(e);
        painter.Cross({t.worldX, t.worldY}, Editor::Palette().Selection);
    }
}

void DrawOverlaysMenu(SpatialOverlayOptions& options) {
    const bool any = options.colliders || options.occluders || options.navAreas;
    if (ToggleIconButton(ICON_FA_LAYER_GROUP, any, "Overlays")) ImGui::OpenPopup("SpatialOverlays");
    if (!ImGui::BeginPopup("SpatialOverlays")) return;
    SectionHeader("Overlays");
    ImGui::Checkbox("Colliders", &options.colliders);
    ImGui::Checkbox("Occluders", &options.occluders);
    ImGui::Checkbox("Nav areas", &options.navAreas);
    MutedText("Selected entities always show theirs");
    ImGui::EndPopup();
}

}  // namespace Elysium

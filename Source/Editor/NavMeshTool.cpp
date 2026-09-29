#include "Editor/NavMeshTool.h"
#include <algorithm>
#include "Components/NameComponent.h"
#include "Components/TransformComponent.h"
#include "Core/Geometry.h"
#include "Core/World.h"
#include "Editor/OverlayPainter.h"
#include "Editor/SpatialOverlays.h"
#include "Editor/Theme.h"
#include "Editor/Widgets.h"
#include "Interfaces/IEditorService.h"
#include "Systems/NavMeshSystem.h"

namespace Elysium {

using EditorStyle::Palette;

namespace {

constexpr float kClosePixels = 10.0f;
constexpr float kDefaultCost = 3.0f;
// Rects the cell overlay may emit in one frame. 4 verts each against ImGui's 16-bit indices
// (65536 verts per draw list), leaving most of the budget for the rest of the overlay.
constexpr int kMaxCellRects = 6000;

Vector2 Centroid(const std::vector<Vector2>& points) {
    Vector2 sum{0, 0};
    for (const auto& p : points) sum += p;
    return sum / (float)points.size();
}

}  // namespace

void NavMeshTool::SetActive(bool active) {
    active_ = active;
    brush_.reset();
    inProgress_.clear();
    handles_.End();
}

void NavMeshTool::DrawToolbar() {
    auto brushButton = [&](const char* icon, NavAreaType type, const char* tip) {
        ImGui::SameLine();
        if (ToggleIconButton(icon, brush_ == type, tip)) {
            brush_ = (brush_ == type) ? std::nullopt : std::optional(type);
            inProgress_.clear();
        }
    };
    brushButton(ICON_FA_DRAW_POLYGON,   NavAreaType::Walkable, "Walkable region");
    brushButton(ICON_FA_BAN,            NavAreaType::Blocked,  "Blocked area");
    brushButton(ICON_FA_WEIGHT_HANGING, NavAreaType::Cost,     "Cost area");

    ImGui::SameLine();
    const char* hint = !brush_             ? "Click an area to select it, drag its handles, Delete removes it"
                     : inProgress_.empty() ? "Click to lay vertices"
                                           : "Right-click, Enter or the first vertex closes; Esc cancels";
    ColoredText(Editor::Palette().TextMuted, hint);
}

std::vector<Vector2> NavMeshTool::WorldPolygon(World& world, Entity area) {
    if (!world.HasComponent<NavAreaComponent>(area) || !world.HasComponent<TransformComponent>(area)) return {};
    const auto& t = world.GetComponent<TransformComponent>(area);
    return TranslatePolygon(world.GetComponent<NavAreaComponent>(area).LocalPolygon(), {t.worldX, t.worldY});
}

void NavMeshTool::SetWorldPolygon(World& world, Entity area, const std::vector<Vector2>& worldPoints) {
    const auto& t = world.GetComponent<TransformComponent>(area);
    world.GetComponent<NavAreaComponent>(area).points = FormatPointList(TranslatePolygon(worldPoints, {-t.worldX, -t.worldY}));
}

std::optional<Entity> NavMeshTool::AreaAt(World& world, Vector2 mouseWorld) const {
    std::optional<Entity> hit;
    float smallest = 1e30f;
    world.Query<TransformComponent, NavAreaComponent>([&](Entity e, auto&, auto&) {
        auto polygon = WorldPolygon(world, e);
        if (polygon.empty() || !PointInPolygon(mouseWorld, polygon)) return;
        Rectangle b = PolygonBounds(polygon);
        if (b.width * b.height < smallest) { smallest = b.width * b.height; hit = e; }
    });
    return hit;
}

void NavMeshTool::ClosePolygon(World& world, Services::IEditorService& editor) {
    std::vector<Vector2> points = std::move(inProgress_);
    inProgress_.clear();
    if (points.size() < 3 || !brush_) return;

    Entity e = editor.CreateEntity();
    if (e == INVALID_ENTITY) return;

    const Vector2 centroid = Centroid(points);
    NavAreaComponent area;
    area.type = ToString(*brush_);
    area.cost = *brush_ == NavAreaType::Cost ? kDefaultCost : 1.0f;
    area.points = FormatPointList(TranslatePolygon(points, centroid * -1.0f));

    TransformComponent transform(centroid.x, centroid.y);
    transform.worldX = centroid.x;
    transform.worldY = centroid.y;

    world.AddComponent<NameComponent>(e, NameComponent(std::string("NavArea ") + area.type));
    world.AddComponent<TransformComponent>(e, transform);
    world.AddComponent<NavAreaComponent>(e, area);
    editor.SelectEntity(e);
}

bool NavMeshTool::HandleInput(World& world, Services::IEditorService& editor, const ViewportInput& in) {
    if (!active_) return false;
    const float handleRadius = OverlayPainter::HandleRadius * in.worldPerPixel;
    const float closeRadius = kClosePixels * in.worldPerPixel;

    if (handles_.Dragging()) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !world.IsAlive(editing_)) { handles_.End(); return true; }
        auto polygon = WorldPolygon(world, editing_);
        handles_.Drag(polygon, in.mouseWorld);
        SetWorldPolygon(world, editing_, polygon);
        return true;
    }
    if (!in.hovered) return false;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (!inProgress_.empty()) inProgress_.clear();
        else brush_.reset();
        return true;
    }

    if (brush_) {
        const bool nearStart = inProgress_.size() >= 3 && (inProgress_.front() - in.mouseWorld).Length() <= closeRadius;
        const bool close = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || in.rightClicked || (in.clicked && nearStart);
        if (close) { ClosePolygon(world, editor); return true; }
        if (in.clicked) { inProgress_.push_back(in.mouseWorld); return true; }
        return false;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        auto selected = editor.GetSelectedEntities();
        for (Entity e : selected) {
            if (world.HasComponent<NavAreaComponent>(e)) editor.DeleteEntity(e);
        }
        return !selected.empty();
    }

    if (!in.clicked) return false;
    for (Entity e : editor.GetSelectedEntities()) {
        if (handles_.Begin(WorldPolygon(world, e), in.mouseWorld, handleRadius)) { editing_ = e; return true; }
    }
    if (auto hit = AreaAt(world, in.mouseWorld)) { editor.SelectEntity(*hit); return true; }
    return false;
}

void NavMeshTool::DrawOverlay(World& world, Services::IEditorService& editor, const Systems::NavMeshSystem* nav, OverlayPainter& painter) {
    if (!active_) return;

    if (nav && nav->Width() > 0) {
        const float cs = nav->CellSize();
        const Rectangle b = nav->Bounds();
        // One rect per horizontal run of same-coloured cells, never one per cell: at a small
        // cellSize a scene has tens of thousands of walkable cells, and ImGui's draw list uses
        // 16-bit indices — 4 verts per rect asserts out somewhere past 16k of them. Runs collapse
        // open ground to a couple of rects per row. The budget is a backstop for a pathological
        // bake (alternating cost cells), where the overlay goes incomplete rather than crashing.
        int budget = kMaxCellRects;
        for (int cy = 0; cy < nav->Height() && budget > 0; ++cy) {
            int runStart = -1;
            bool runCost = false;
            auto flush = [&](int endX) {
                if (runStart < 0) return;
                const ImVec4& color = runCost ? Editor::Palette().Warning : Editor::Palette().Success;
                painter.Rect({b.x + runStart * cs, b.y + cy * cs}, {b.x + endX * cs, b.y + (cy + 1) * cs}, color, 0.22f, 0.0f);
                --budget;
                runStart = -1;
            };
            for (int cx = 0; cx < nav->Width() && budget > 0; ++cx) {
                const auto* cell = nav->GetCell(cx, cy);
                const bool walkable = cell && cell->walkable;
                const bool cost = walkable && cell->cost > 1.0f;
                if (walkable && runStart >= 0 && cost == runCost) continue;  // extend the run
                flush(cx);
                if (walkable) { runStart = cx; runCost = cost; }
            }
            flush(nav->Width());
        }
        painter.Rect({b.x, b.y}, {b.x + b.width, b.y + b.height}, Palette::WithAlpha(Editor::Palette().TextMuted, 0.5f));
    }

    world.Query<TransformComponent, NavAreaComponent>([&](Entity e, auto&, auto& area) {
        auto polygon = WorldPolygon(world, e);
        if (polygon.empty()) return;
        const ImVec4& color = NavAreaColor(area.Type());
        const bool selected = editor.IsSelected(e);
        painter.Polygon(polygon, color, selected ? 0.25f : 0.12f, selected ? painter.LineWidth() + 1.0f : 0.0f);
        if (selected) painter.Handles(polygon, Editor::Palette().Selection);
    });

    if (!inProgress_.empty() && brush_) {
        const ImVec4& color = NavAreaColor(*brush_);
        painter.Polygon(inProgress_, color, 0.0f, 0.0f, false);
        for (const auto& p : inProgress_) painter.Circle(p, 3.0f, color, true);
        painter.Circle(inProgress_.front(), kClosePixels, color);
    }
}

}  // namespace Elysium

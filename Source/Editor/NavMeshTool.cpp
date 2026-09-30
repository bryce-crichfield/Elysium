#include "Editor/NavMeshTool.h"
#include "extras/IconsFontAwesome6.h"
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
// Rects the cell overlay may emit in one frame. 4 verts each against ImGui's 16-bit indices
// (65536 verts per draw list), leaving most of the budget for the rest of the overlay.
constexpr int kMaxCellRects = 6000;

Vector2 Centroid(const std::vector<Vector2>& points) {
    Vector2 sum{0, 0};
    for (const auto& p : points) sum += p;
    return sum / (float)points.size();
}

}  // namespace

const char* NavMeshTool::Icon() const { return ICON_FA_ROUTE; }

const char* NavMeshTool::Unavailable(Services::IEditorService&, bool isScene) const {
    return isScene ? nullptr : "Navmesh editing needs a scene";
}

void NavMeshTool::OnActivate(Services::IEditorService&) {
    brushChoice_ = 0;
    inProgress_.clear();
}

void NavMeshTool::OnDeactivate(Services::IEditorService&) {
    brushChoice_ = 0;
    inProgress_.clear();
}

ToolStatus NavMeshTool::Status(Services::IEditorService&) const {
    if (!Brush()) {
        return {"Pick what to draw in the tool settings " ICON_FA_WRENCH
                ", or click an area to select it (Delete removes it)"};
    }
    if (inProgress_.empty()) return {"Click to lay vertices", ToolStatusLevel::Working};
    return {"Right-click, Enter or the first vertex closes; Esc cancels", ToolStatusLevel::Working};
}

std::vector<Vector2> NavMeshTool::WorldPolygon(World& world, Entity area) {
    if (!world.HasComponent<NavAreaComponent>(area) || !world.HasComponent<TransformComponent>(area)) return {};
    const auto& t = world.GetComponent<TransformComponent>(area);
    return TranslatePolygon(world.GetComponent<NavAreaComponent>(area).LocalPolygon(), {t.worldX, t.worldY});
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
    const std::optional<NavAreaType> brush = Brush();
    if (points.size() < 3 || !brush) return;

    editor.BeginTransaction("Add Nav Area");
    Entity e = editor.CreateEntity();
    if (e == INVALID_ENTITY) {
        editor.EndTransaction();
        return;
    }

    const Vector2 centroid = Centroid(points);
    NavAreaComponent area;
    area.type = ToString(*brush);
    area.cost = *brush == NavAreaType::Cost ? cost_ : 1.0f;
    area.points = FormatPointList(TranslatePolygon(points, centroid * -1.0f));

    TransformComponent transform(centroid.x, centroid.y);
    transform.worldX = centroid.x;
    transform.worldY = centroid.y;

    world.AddComponent<NameComponent>(e, NameComponent(std::string("NavArea ") + area.type));
    world.AddComponent<TransformComponent>(e, transform);
    world.AddComponent<NavAreaComponent>(e, area);
    editor.SelectEntity(e);
    editor.EndTransaction();
}

bool NavMeshTool::HandleInput(ToolContext& context) {
    World& world = context.world;
    Services::IEditorService& editor = context.editor;
    const ViewportInput& in = context.input;
    const float closeRadius = kClosePixels * in.worldPerPixel;

    // A brush change from the tool panel abandons whatever was half-laid: the vertices so far
    // belong to the area type they were started under.
    if (brushChoice_ != lastBrushChoice_) {
        lastBrushChoice_ = brushChoice_;
        inProgress_.clear();
    }

    if (!in.hovered) return false;

    // Escape backs out one step at a time: the polygon being laid, then the brush. Only claimed
    // when there is something to cancel, so a third press still means "leave the tool".
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && (!inProgress_.empty() || Brush())) {
        if (!inProgress_.empty()) inProgress_.clear();
        else brushChoice_ = 0;
        return true;
    }

    if (Brush()) {
        const bool nearStart = inProgress_.size() >= 3 && (inProgress_.front() - in.mouseWorld).Length() <= closeRadius;
        const bool close = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || in.rightClicked || (in.clicked && nearStart);
        if (close) { ClosePolygon(world, editor); return true; }
        if (in.clicked) { inProgress_.push_back(in.mouseWorld); return true; }
        return false;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        auto selected = editor.GetSelectedEntities();
        editor.BeginTransaction("Delete Nav Areas");
        for (Entity e : selected) {
            if (world.HasComponent<NavAreaComponent>(e)) editor.DeleteEntity(e);
        }
        editor.EndTransaction();
        return !selected.empty();
    }

    if (!in.clicked) return false;
    if (auto hit = AreaAt(world, in.mouseWorld)) { editor.SelectEntity(*hit); return true; }
    return false;
}

void NavMeshTool::DrawOverlay(ToolContext& context, OverlayPainter& painter) {
    World& world = context.world;
    Services::IEditorService& editor = context.editor;
    const Systems::NavMeshSystem* nav = context.nav;

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
    });

    if (!inProgress_.empty() && Brush()) {
        const ImVec4& color = NavAreaColor(*Brush());
        painter.Polygon(inProgress_, color, 0.0f, 0.0f, false);
        for (const auto& p : inProgress_) painter.Circle(p, 3.0f, color, true);
        painter.Circle(inProgress_.front(), kClosePixels, color);
    }
}

}  // namespace Elysium

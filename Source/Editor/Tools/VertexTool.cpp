#include "Editor/Tools/VertexTool.h"

#include <cmath>
#include <memory>
#include <string>
#include "Core/Components/ColliderComponent.h"
#include "Core/Components/TransformComponent.h"
#include "Editor/Editor.h"
#include "Core/EntitySerializer.h"
#include "Core/Math/Polygon.h"
#include "Core/World.h"
#include "Editor/Commands/EditorCommands.h"
#include "Editor/Viewport/OverlayPainter.h"
#include "Editor/Style/Theme.h"
#include "Editor/Widgets/Widgets.h"
#include "extras/IconsFontAwesome6.h"
#include "Editor/EditorApplication.h"
#include "imgui.h"

namespace Elysium {

namespace {

// How close to an edge a click counts as "on it", in pixels.
constexpr float kEdgePixels = 8.0f;

// A polygon is always stored local to its entity's transform, so moving the entity moves the
// shape with it. Editing happens in world space, which means translating both ways around every
// change.
Vector2 OriginOf(World& world, Entity entity) {
    if (!world.HasComponent<TransformComponent>(entity)) return {0.0f, 0.0f};
    const auto& t = world.GetComponent<TransformComponent>(entity);
    return {t.worldX, t.worldY};
}

Polygon ReadPolygon(World& world, Entity entity) {
    if (!world.HasComponent<ColliderComponent>(entity)) return {};
    return world.GetComponent<ColliderComponent>(entity).LocalPolygon();
}

void WritePolygon(World& world, Entity entity, const Polygon& local) {
    if (!world.HasComponent<ColliderComponent>(entity)) return;
    auto& collider = world.GetComponent<ColliderComponent>(entity);
    collider.points = local.Format();
    // A reshaped collider is a polygon collider, and its box has to follow, or the Auto shape
    // and the broadphase bounds would still describe the outline you just replaced.
    collider.shape = ToString(ColliderShape::Polygon);
    collider.SyncBoxToPolygon();
}

// The edge `point` is nearest to, when it is within `radius` of it, and where on that edge it lands.
struct EdgeHit {
    size_t after = 0;  // insert the new vertex after this index
    Vector2 at{};
};

std::optional<EdgeHit> NearestEdge(const Polygon& polygon, Vector2 point, float radius) {
    const auto nearest = polygon.NearestOnOutline(point);
    if (!nearest || nearest->distance >= radius) return std::nullopt;
    return EdgeHit{nearest->edge, nearest->point};
}

}  // namespace

const char* VertexTool::Icon() const { return ICON_FA_VECTOR_SQUARE; }

const char* VertexTool::Unavailable(EditorApplication& editor, bool isScene) const {
    (void)isScene;  // a collider is editable on a prefab tab too
    World* world = editor.GetWorld();
    if (!world) return "Open a scene or prefab to edit outlines";
    for (Entity entity : editor.GetSelectedEntities()) {
        if (world->IsAlive(entity) && ReadPolygon(*world, entity).IsValid()) return nullptr;
    }
    return "Select a collider to reshape it";
}

void VertexTool::OnDeactivate(EditorApplication&) {
    handles_.End();
    dragEntity_ = INVALID_ENTITY;
}

ToolStatus VertexTool::Status(EditorApplication& editor) const {
    World* world = editor.GetWorld();
    if (!world) return {};
    int count = 0;
    for (Entity entity : editor.GetSelectedEntities()) {
        if (world->IsAlive(entity) && ReadPolygon(*world, entity).IsValid()) count++;
    }

    if (count == 0) return {"The selection has no collider", ToolStatusLevel::Warning};
    const std::string what = count == 1 ? "1 collider" : std::to_string(count) + " colliders";
    return {"Reshaping " + what + ": drag a vertex, click an edge to add one, right-click to remove",
            ToolStatusLevel::Working};
}

std::vector<VertexTool::Target> VertexTool::TargetsOf(ToolContext& context) const {
    std::vector<Target> targets;
    for (Entity entity : context.editor.GetSelectedEntities()) {
        if (!context.world.IsAlive(entity)) continue;
        Polygon local = ReadPolygon(context.world, entity);
        if (!local.IsValid()) continue;
        targets.push_back(Target{entity, ColliderComponent::XmlTag(), local.Translated(OriginOf(context.world, entity))});
    }
    return targets;
}

void VertexTool::Commit(ToolContext& context, const Target& target, const Polygon& world,
                        const std::string& before, const char* label) {
    const Vector2 origin = OriginOf(context.world, target.entity);
    WritePolygon(context.world, target.entity, world.Translated(origin * -1.0f));

    std::string after = EntityXml::SaveComponent(context.world, target.entity, target.component);
    if (after == before) return;

    context.editor.Execute(std::make_unique<ComponentEditCommand>(
        EntityRef{context.editor.StableIdOf(target.entity)}, target.component, before, std::move(after), label));
}

bool VertexTool::HandleInput(ToolContext& context) {
    const ViewportInput& in = context.input;
    const float handleRadius = OverlayPainter::HandleRadius * in.worldPerPixel;
    const float edgeRadius = kEdgePixels * in.worldPerPixel;

    // A drag in progress writes every frame for live feedback, but only records once, on
    // release, against the shape the drag started from.
    if (handles_.Dragging()) {
        if (!context.world.IsAlive(dragEntity_)) {
            handles_.End();
            dragEntity_ = INVALID_ENTITY;
            return true;
        }

        const Vector2 origin = OriginOf(context.world, dragEntity_);
        Polygon world = ReadPolygon(context.world, dragEntity_).Translated(origin);
        handles_.Drag(world.Points(), in.mouseWorld);

        const Target target{dragEntity_, dragComponent_, world};
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Live, unrecorded: write it straight through without a command.
            WritePolygon(context.world, dragEntity_, world.Translated(origin * -1.0f));
            return true;
        }

        Commit(context, target, world, dragBefore_, "Move Vertex");
        handles_.End();
        dragEntity_ = INVALID_ENTITY;
        return true;
    }

    if (!in.hovered) return false;

    std::vector<Target> targets = TargetsOf(context);

    // Right-click a vertex to remove it, down to the three a polygon needs.
    if (in.rightClicked) {
        for (const Target& target : targets) {
            auto vertex = PolygonHandles::HitVertex(target.world.Points(), in.mouseWorld, handleRadius);
            if (!vertex) continue;
            if (target.world.Size() <= 3) return true;  // any fewer stops being a polygon

            const std::string before = EntityXml::SaveComponent(context.world, target.entity, target.component);
            Polygon world = target.world;
            world.Points().erase(world.Points().begin() + (long)*vertex);
            Commit(context, target, world, before, "Remove Vertex");
            return true;
        }
        return false;
    }

    if (!in.clicked) return false;

    // A vertex under the cursor starts a drag.
    for (const Target& target : targets) {
        if (!handles_.Begin(target.world.Points(), in.mouseWorld, handleRadius)) continue;
        dragEntity_ = target.entity;
        dragComponent_ = target.component;
        dragBefore_ = EntityXml::SaveComponent(context.world, target.entity, target.component);
        return true;
    }

    // Otherwise an edge under the cursor gains one, and the new vertex is picked up immediately
    // so adding and positioning it is one motion.
    for (const Target& target : targets) {
        const auto edge = NearestEdge(target.world, in.mouseWorld, edgeRadius);
        if (!edge) continue;

        const std::string before = EntityXml::SaveComponent(context.world, target.entity, target.component);
        Polygon world = target.world;
        world.Points().insert(world.Points().begin() + (long)edge->after + 1, edge->at);
        Commit(context, target, world, before, "Add Vertex");

        dragEntity_ = target.entity;
        dragComponent_ = target.component;
        dragBefore_ = EntityXml::SaveComponent(context.world, target.entity, target.component);
        handles_.Begin(world.Points(), edge->at, handleRadius);
        return true;
    }

    // Nothing of ours here: fall through so the click still picks, and another collider can be
    // selected without leaving the tool.
    const std::vector<Entity> hits = context.pick();
    if (hits.empty()) return false;
    context.editor.SelectEntity(hits.front(), ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl);
    return true;
}

void VertexTool::DrawOverlay(ToolContext& context, OverlayPainter& painter) {
    for (const Target& target : TargetsOf(context)) {
        painter.Polygon(target.world.Points(), Editor::Palette().Selection, 0.08f, painter.LineWidth() + 1.0f);
        painter.Handles(target.world.Points(), Editor::Palette().Selection);

        // Preview where a click would add a vertex, so edges read as clickable.
        if (!context.input.hovered || handles_.Dragging()) continue;
        const auto edge = NearestEdge(target.world, context.input.mouseWorld, kEdgePixels * context.input.worldPerPixel);
        if (edge) painter.Circle(edge->at, OverlayPainter::HandleRadius * 0.6f, Editor::Palette().Accent, true);
    }
}

}  // namespace Elysium

#include "Editor/Tools/VertexTool.h"

#include <cmath>
#include <memory>
#include <string>
#include "Components/ColliderComponent.h"
#include "Components/NavAreaComponent.h"
#include "Components/OccluderComponent.h"
#include "Components/TransformComponent.h"
#include "Core/Editor.h"
#include "Core/EntitySerializer.h"
#include "Core/Geometry.h"
#include "Core/World.h"
#include "Editor/Commands/EditorCommands.h"
#include "Editor/OverlayPainter.h"
#include "Editor/Theme.h"
#include "Editor/Widgets.h"
#include "extras/IconsFontAwesome6.h"
#include "Interfaces/IEditorService.h"
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

// The polygon-bearing components this tool understands, in the order the Shape parameter lists
// them. Each is a local point list on an entity with a transform, so all three reshape
// identically; only the field differs. kShapeLabels is what the UI calls them.
const char* const kPolygonComponents[] = {NavAreaComponent::XmlTag(), OccluderComponent::XmlTag(),
                                          ColliderComponent::XmlTag()};
const char* const kShapeLabels[] = {"nav area", "occluder", "collider"};
constexpr int kShapeCount = (int)(sizeof(kPolygonComponents) / sizeof(kPolygonComponents[0]));

std::vector<Vector2> ReadPolygon(World& world, Entity entity, const std::string& tag) {
    if (tag == NavAreaComponent::XmlTag() && world.HasComponent<NavAreaComponent>(entity)) {
        return world.GetComponent<NavAreaComponent>(entity).LocalPolygon();
    }
    if (tag == OccluderComponent::XmlTag() && world.HasComponent<OccluderComponent>(entity)) {
        return world.GetComponent<OccluderComponent>(entity).LocalFootprint();
    }
    if (tag == ColliderComponent::XmlTag() && world.HasComponent<ColliderComponent>(entity)) {
        return world.GetComponent<ColliderComponent>(entity).LocalPolygon();
    }
    return {};
}

void WritePolygon(World& world, Entity entity, const std::string& tag, const std::vector<Vector2>& local) {
    const std::string text = FormatPointList(local);
    if (tag == NavAreaComponent::XmlTag() && world.HasComponent<NavAreaComponent>(entity)) {
        world.GetComponent<NavAreaComponent>(entity).points = text;
    } else if (tag == OccluderComponent::XmlTag() && world.HasComponent<OccluderComponent>(entity)) {
        world.GetComponent<OccluderComponent>(entity).footprint = text;
    } else if (tag == ColliderComponent::XmlTag() && world.HasComponent<ColliderComponent>(entity)) {
        auto& collider = world.GetComponent<ColliderComponent>(entity);
        collider.points = text;
        // A reshaped collider is a polygon collider, and its box has to follow, or the Auto shape
        // and the broadphase bounds would still describe the outline you just replaced.
        collider.shape = ToString(ColliderShape::Polygon);
        collider.SyncBoxToPolygon();
    }
}

// Index of the edge `point` is nearest to (the edge from result to result+1), when it is within
// `radius` of it, plus where on that edge it lands.
struct EdgeHit {
    size_t after = 0;  // insert the new vertex after this index
    Vector2 at{};
    bool found = false;
};

EdgeHit NearestEdge(const std::vector<Vector2>& polygon, Vector2 point, float radius) {
    EdgeHit best;
    float bestDistance = radius;

    for (size_t i = 0; i < polygon.size(); i++) {
        const Vector2 a = polygon[i], b = polygon[(i + 1) % polygon.size()];
        const Vector2 edge = b - a;
        const float lengthSquared = edge.x * edge.x + edge.y * edge.y;
        if (lengthSquared <= 0.0f) continue;

        // Projection of the point onto the segment, clamped to it.
        float t = ((point.x - a.x) * edge.x + (point.y - a.y) * edge.y) / lengthSquared;
        t = std::fmin(1.0f, std::fmax(0.0f, t));
        const Vector2 closest{a.x + edge.x * t, a.y + edge.y * t};

        const float distance = (closest - point).Length();
        if (distance >= bestDistance) continue;
        bestDistance = distance;
        best = EdgeHit{i, closest, true};
    }
    return best;
}

}  // namespace

const char* VertexTool::Icon() const { return ICON_FA_VECTOR_SQUARE; }

const char* VertexTool::Unavailable(Services::IEditorService& editor, bool isScene) const {
    (void)isScene;  // an occluder or nav area is editable on a prefab tab too
    World* world = editor.GetWorld();
    if (!world) return "Open a scene or prefab to edit outlines";

    // Deliberately any of the three kinds, not the chosen one: the choice lives in this tool's own
    // panel, so gating on it would make the tool unselectable and the choice unreachable. A
    // selection carrying no polygon of the chosen kind leaves the tool selectable and says so.
    for (Entity entity : editor.GetSelectedEntities()) {
        if (!world->IsAlive(entity)) continue;
        for (const char* tag : kPolygonComponents) {
            if (ReadPolygon(*world, entity, tag).size() >= 3) return nullptr;
        }
    }
    return "Select a nav area, occluder or collider to reshape it";
}

void VertexTool::OnDeactivate(Services::IEditorService&) {
    handles_.End();
    dragEntity_ = INVALID_ENTITY;
}

const char* VertexTool::ShapeTag() const {
    return kPolygonComponents[shape_ >= 0 && shape_ < kShapeCount ? shape_ : 0];
}

ToolStatus VertexTool::Status(Services::IEditorService& editor) const {
    World* world = editor.GetWorld();
    if (!world) return {};

    // Name what is actually being reshaped. Not knowing that was the confusing part: an entity can
    // carry a nav area, an occluder and a collider at once, and three outlines in the same colour
    // said nothing about which one a drag would move.
    const char* label = kShapeLabels[shape_ >= 0 && shape_ < kShapeCount ? shape_ : 0];
    const char* tag = ShapeTag();
    int count = 0;
    for (Entity entity : editor.GetSelectedEntities()) {
        if (world->IsAlive(entity) && ReadPolygon(*world, entity, tag).size() >= 3) count++;
    }

    if (count == 0) {
        return {std::string("The selection has no ") + label +
                    " - pick another shape in the tool settings " ICON_FA_WRENCH,
                ToolStatusLevel::Warning};
    }
    const std::string what = count == 1 ? std::string("1 ") + label : std::to_string(count) + " " + label + "s";
    return {"Reshaping " + what + ": drag a vertex, click an edge to add one, right-click to remove",
            ToolStatusLevel::Working};
}

std::vector<VertexTool::Target> VertexTool::TargetsOf(ToolContext& context) const {
    // Only the chosen kind. Editing every polygon the selection carried meant a drag could land on
    // a component you were not looking at.
    const char* tag = ShapeTag();
    std::vector<Target> targets;
    for (Entity entity : context.editor.GetSelectedEntities()) {
        if (!context.world.IsAlive(entity)) continue;
        std::vector<Vector2> local = ReadPolygon(context.world, entity, tag);
        if (local.size() < 3) continue;
        targets.push_back(Target{entity, tag, TranslatePolygon(local, OriginOf(context.world, entity))});
    }
    return targets;
}

void VertexTool::Commit(ToolContext& context, const Target& target, const std::vector<Vector2>& world,
                        const std::string& before, const char* label) {
    const Vector2 origin = OriginOf(context.world, target.entity);
    WritePolygon(context.world, target.entity, target.component, TranslatePolygon(world, origin * -1.0f));

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
        std::vector<Vector2> world = TranslatePolygon(ReadPolygon(context.world, dragEntity_, dragComponent_), origin);
        handles_.Drag(world, in.mouseWorld);

        const Target target{dragEntity_, dragComponent_, world};
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Live, unrecorded: write it straight through without a command.
            WritePolygon(context.world, dragEntity_, dragComponent_, TranslatePolygon(world, origin * -1.0f));
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
            auto vertex = PolygonHandles::HitVertex(target.world, in.mouseWorld, handleRadius);
            if (!vertex) continue;
            if (target.world.size() <= 3) return true;  // any fewer stops being a polygon

            const std::string before = EntityXml::SaveComponent(context.world, target.entity, target.component);
            std::vector<Vector2> world = target.world;
            world.erase(world.begin() + (long)*vertex);
            Commit(context, target, world, before, "Remove Vertex");
            return true;
        }
        return false;
    }

    if (!in.clicked) return false;

    // A vertex under the cursor starts a drag.
    for (const Target& target : targets) {
        if (!handles_.Begin(target.world, in.mouseWorld, handleRadius)) continue;
        dragEntity_ = target.entity;
        dragComponent_ = target.component;
        dragBefore_ = EntityXml::SaveComponent(context.world, target.entity, target.component);
        return true;
    }

    // Otherwise an edge under the cursor gains one, and the new vertex is picked up immediately
    // so adding and positioning it is one motion.
    for (const Target& target : targets) {
        const EdgeHit edge = NearestEdge(target.world, in.mouseWorld, edgeRadius);
        if (!edge.found) continue;

        const std::string before = EntityXml::SaveComponent(context.world, target.entity, target.component);
        std::vector<Vector2> world = target.world;
        world.insert(world.begin() + (long)edge.after + 1, edge.at);
        Commit(context, target, world, before, "Add Vertex");

        dragEntity_ = target.entity;
        dragComponent_ = target.component;
        dragBefore_ = EntityXml::SaveComponent(context.world, target.entity, target.component);
        handles_.Begin(world, edge.at, handleRadius);
        return true;
    }

    // Nothing of ours here: fall through so the click still picks, and a nav area can be
    // selected without leaving the tool.
    const std::vector<Entity> hits = context.pick();
    if (hits.empty()) return false;
    context.editor.SelectEntity(hits.front(), ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl);
    return true;
}

void VertexTool::DrawOverlay(ToolContext& context, OverlayPainter& painter) {
    for (const Target& target : TargetsOf(context)) {
        painter.Polygon(target.world, Editor::Palette().Selection, 0.08f, painter.LineWidth() + 1.0f);
        painter.Handles(target.world, Editor::Palette().Selection);

        // Preview where a click would add a vertex, so edges read as clickable.
        if (!context.input.hovered || handles_.Dragging()) continue;
        const EdgeHit edge = NearestEdge(target.world, context.input.mouseWorld, kEdgePixels * context.input.worldPerPixel);
        if (edge.found) painter.Circle(edge.at, OverlayPainter::HandleRadius * 0.6f, Editor::Palette().Accent, true);
    }
}

}  // namespace Elysium

#pragma once

#include <string>
#include <vector>
#include "Core/Entity.h"
#include "Core/Math/Polygon.h"
#include "Core/Math/MathTypes.h"
#include "Editor/Tools/PolygonHandles.h"
#include "Editor/Tools/ViewportTool.h"

namespace Elysium {

// Reshapes one kind of polygon on the selected entities: a nav area's region or a
// collider's outline. Until now a closed shape was final -- you could lay one out
// and never correct it.
//
// The kind is a tool setting rather than "whatever the selection happens to carry", because an
// entity can carry several: editing all of them at once drew identically-coloured outlines
// on top of each other with no way to tell which vertex belonged to what. One kind at a time, named
// in the panel and in the status line.
//
// Drag a vertex to move it, click an edge to add one, right-click a vertex to remove it. Each
// of those is one undo step; a drag is one step for the whole drag, not one per frame.
class VertexTool : public ViewportTool {
   public:
    const char* Name() const override { return "Vertices"; }
    const char* Icon() const override;
    const char* Tooltip() const override {
        return "Edit vertices (5) - drag to move, click an edge to add, right-click a vertex to remove";
    }

    // Nothing to reshape without a polygon selected, so the tool stays unselectable until one
    // is -- which in practice means after the navmesh tool picked a nav area.
    const char* Unavailable(EditorApplication& editor, bool isScene) const override;

    ToolParameters Parameters() override {
        return {this, {Field("Shape", &VertexTool::shape_, "shape").Choices({"Nav area", "Collider"})}};
    }

    void OnDeactivate(EditorApplication& editor) override;
    ToolStatus Status(EditorApplication& editor) const override;
    void DrawOverlay(ToolContext& context, OverlayPainter& painter) override;
    bool HandleInput(ToolContext& context) override;

   private:
    // A polygon the tool can edit: which entity, which component, and the points in world space.
    struct Target {
        Entity entity = INVALID_ENTITY;
        std::string component;  // XML tag, which is also what the undo command records
        Polygon world;

        bool Valid() const { return entity != INVALID_ENTITY && world.IsValid(); }
    };

    // The XML tag of the component `shape_` names.
    const char* ShapeTag() const;
    // The chosen kind of polygon on each selected entity that has one.
    std::vector<Target> TargetsOf(ToolContext& context) const;
    // Writes `world` back onto the target and records the change against `before`.
    static void Commit(ToolContext& context, const Target& target, const Polygon& world,
                       const std::string& before, const char* label);

    // Which polygon-bearing component to reshape, as an index into the Parameters() choices.
    int shape_ = 0;

    // The polygon being dragged, and its serialized component as the drag found it.
    Entity dragEntity_ = INVALID_ENTITY;
    std::string dragComponent_;
    std::string dragBefore_;
    PolygonHandles handles_;
};

}  // namespace Elysium

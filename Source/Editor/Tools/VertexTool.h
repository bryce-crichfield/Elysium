#pragma once

#include <string>
#include <vector>
#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include "Editor/PolygonHandles.h"
#include "Editor/Tools/ViewportTool.h"

namespace Elysium {

// Reshapes the polygon of whichever polygon-bearing component is selected: a nav area's region
// or an occluder's footprint. Until now a closed shape was final — you could lay one out and
// never correct it.
//
// Drag a vertex to move it, click an edge to add one, right-click a vertex to remove it. Each
// of those is one undo step; a drag is one step for the whole drag, not one per frame.
class VertexTool : public ViewportTool {
   public:
    const char* Name() const override { return "Vertices"; }
    const char* Icon() const override;
    const char* Tooltip() const override {
        return "Edit vertices (4) - drag to move, click an edge to add, right-click a vertex to remove";
    }

    void OnDeactivate(Services::IEditorService& editor) override;
    void DrawToolbar(Services::IEditorService& editor) override;
    void DrawOverlay(ToolContext& context, OverlayPainter& painter) override;
    bool HandleInput(ToolContext& context) override;

   private:
    // A polygon the tool can edit: which entity, which component, and the points in world space.
    struct Target {
        Entity entity = INVALID_ENTITY;
        std::string component;  // XML tag, which is also what the undo command records
        std::vector<Vector2> world;

        bool Valid() const { return entity != INVALID_ENTITY && world.size() >= 3; }
    };

    // Every editable polygon on the selected entities.
    static std::vector<Target> TargetsOf(ToolContext& context);
    // Writes `world` back onto the target and records the change against `before`.
    static void Commit(ToolContext& context, const Target& target, const std::vector<Vector2>& world,
                       const std::string& before, const char* label);

    // The polygon being dragged, and its serialized component as the drag found it.
    Entity dragEntity_ = INVALID_ENTITY;
    std::string dragComponent_;
    std::string dragBefore_;
    PolygonHandles handles_;
};

}  // namespace Elysium

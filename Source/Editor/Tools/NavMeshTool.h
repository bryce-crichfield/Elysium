#pragma once
#include <optional>
#include <vector>
#include "Core/Components/NavAreaComponent.h"
#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include "Editor/Tools/ViewportTool.h"

namespace Elysium {
class World;
class OverlayPainter;
namespace Services { class IEditorService; }

// Viewport navmesh mode: shows the bake and lays out NavArea entities.
//
// Laying out and reshaping are deliberately different tools. This one draws new areas and picks
// or deletes existing ones; correcting an outline afterwards is the vertex tool's job, which
// does the same thing for collider outlines and would only have been duplicated here.
class NavMeshTool : public ViewportTool {
public:
    const char* Name() const override { return "Navmesh"; }
    const char* Icon() const override;
    const char* Tooltip() const override { return "Navmesh (7) - lay out walkable, blocked and cost areas"; }

    // The generic nav-area overlay would draw underneath this tool's own.
    bool OwnsNavAreaOverlay() const override { return true; }
    const char* Unavailable(Services::IEditorService& editor, bool isScene) const override;

    void OnActivate(Services::IEditorService& editor) override;
    void OnDeactivate(Services::IEditorService& editor) override;

    ToolParameters Parameters() override {
        return {this,
                {Field("Draw", &NavMeshTool::brushChoice_, "brush")
                     .Choices({"Select / delete", "Walkable", "Blocked", "Cost"}),
                 Field("Cost", &NavMeshTool::cost_, "cost").Range(1.0f, 10.0f).Speed(0.1f)}};
    }

    ToolStatus Status(Services::IEditorService& editor) const override;
    void DrawOverlay(ToolContext& context, OverlayPainter& painter) override;
    bool HandleInput(ToolContext& context) override;

private:
    void ClosePolygon(World& world, Services::IEditorService& editor);
    std::optional<Entity> AreaAt(World& world, Vector2 mouseWorld) const;

    static Polygon WorldPolygon(World& world, Entity area);

    // Which brush is armed, as an index into the Parameters() choices: 0 is select/delete, and
    // 1..3 are the area types in NavAreaType order. It lives as an int because that is what a
    // Choice field is; Brush() is what the rest of the tool reads.
    //
    // This was three toggle buttons the tool drew onto the toolbar itself. It is a tool setting
    // like any other, so it belongs in the tool panel with the cost it pairs with -- and being a
    // parameter means it needs no UI code at all.
    std::optional<NavAreaType> Brush() const {
        return brushChoice_ >= 1 && brushChoice_ <= 3 ? std::optional((NavAreaType)(brushChoice_ - 1))
                                                      : std::nullopt;
    }

    int brushChoice_ = 0;
    // The brush as of last frame. Switching brushes abandons a half-laid polygon, which the old
    // toggle buttons did inline; now that the brush is a panel field, the change has to be noticed.
    int lastBrushChoice_ = 0;
    std::vector<Vector2> inProgress_;
    // Traversal cost given to a new Cost area. Ignored by the walkable and blocked brushes,
    // whose cost is fixed by what they mean.
    float cost_ = 3.0f;
};

}  // namespace Elysium

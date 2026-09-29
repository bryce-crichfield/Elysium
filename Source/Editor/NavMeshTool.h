#pragma once
#include <optional>
#include <vector>
#include "Components/NavAreaComponent.h"
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
// does the same thing for occluder footprints and would only have been duplicated here.
class NavMeshTool : public ViewportTool {
public:
    const char* Name() const override { return "Navmesh"; }
    const char* Icon() const override;
    const char* Tooltip() const override { return "Navmesh (3) - lay out walkable, blocked and cost areas"; }

    // The generic nav-area overlay would draw underneath this tool's own.
    bool OwnsNavAreaOverlay() const override { return true; }
    const char* Unavailable(Services::IEditorService& editor, bool isScene) const override;

    void OnActivate(Services::IEditorService& editor) override;
    void OnDeactivate(Services::IEditorService& editor) override;

    void DrawToolbar(Services::IEditorService& editor) override;
    void DrawOverlay(ToolContext& context, OverlayPainter& painter) override;
    bool HandleInput(ToolContext& context) override;

private:
    void ClosePolygon(World& world, Services::IEditorService& editor);
    std::optional<Entity> AreaAt(World& world, Vector2 mouseWorld) const;

    static std::vector<Vector2> WorldPolygon(World& world, Entity area);

    std::optional<NavAreaType> brush_;
    std::vector<Vector2> inProgress_;
};

}  // namespace Elysium

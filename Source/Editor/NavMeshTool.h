#pragma once
#include <optional>
#include <vector>
#include "Components/NavAreaComponent.h"
#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include "Editor/PolygonHandles.h"

namespace Elysium {
class World;
class OverlayPainter;
namespace Services { class IEditorService; }
namespace Systems { class NavMeshSystem; }

struct ViewportInput {
    Vector2 mouseWorld;
    float worldPerPixel = 1.0f;
    bool hovered = false;
    bool clicked = false;
    bool rightClicked = false;
};

// Viewport navmesh mode: shows the bake and paints NavArea entities.
class NavMeshTool {
public:
    bool IsActive() const { return active_; }
    void SetActive(bool active);

    void DrawToolbar();
    void DrawOverlay(World& world, Services::IEditorService& editor, const Systems::NavMeshSystem* nav, OverlayPainter& painter);
    bool HandleInput(World& world, Services::IEditorService& editor, const ViewportInput& input);

private:
    void ClosePolygon(World& world, Services::IEditorService& editor);
    std::optional<Entity> AreaAt(World& world, Vector2 mouseWorld) const;

    static std::vector<Vector2> WorldPolygon(World& world, Entity area);
    static void SetWorldPolygon(World& world, Entity area, const std::vector<Vector2>& worldPoints);

    bool active_ = false;
    std::optional<NavAreaType> brush_;
    std::vector<Vector2> inProgress_;
    PolygonHandles handles_;
    Entity editing_ = INVALID_ENTITY;
};

}  // namespace Elysium

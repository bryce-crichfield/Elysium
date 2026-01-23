#pragma once

#include "Core/Entity.h"
#include "Core/Event.h"
#include "Core/System.h"

namespace Elysium::Systems {

struct PickEvent : public Event {
    enum class Type {
        PRESS,
        MOVE,
        RELEASE
    };

    PickEvent(Type t, Vector2 pos, int b = 0)
        : type(t), position(pos), button(b) {}

    Type type;
    Vector2 position;
    int button;
};

/**
 * PickSystem handles mouse selection of entities with SelectionComponent and BoundsComponent.
 * Supports:
 * - Single-click selection (click entity to select, deselects others)
 * - Shift+click to add to selection
 * - Box selection (click empty space and drag to draw selection rectangle)
 */
class PickSystem : public System, public IWorldListener, public IMouseListener {
   public:
    PickSystem(Context context);
    ~PickSystem();

    void Update(float deltaTime) override;
    void Draw() override;

    // IEventListener
    void OnEvent(Event& event) override;

    // IWorldListener
    void OnEntityCreated(Entity entity) override {}
    void OnEntityDestroyed(Entity entity) override {}

    // IMouseListener overrides
    void OnMouseButtonPressed(MouseButtonPressedEvent& event) override;
    void OnMouseButtonReleased(MouseButtonReleasedEvent& event) override;
    void OnMouseMoved(MouseMovedEvent& event) override;

   private:
    // Box selection state
    bool isBoxSelecting_ = false;
    Vector2 boxStart_ = {0, 0};      // World space start of selection box
    Vector2 boxCurrent_ = {0, 0};    // World space current mouse position

    // Check if a point is inside an entity's bounds
    bool IsPointInBounds(Vector2 point, const Rectangle& bounds);

    // Check if bounds intersects with selection box
    bool BoundsIntersectsBox(const Rectangle& bounds, const Rectangle& box);

    // Transform framebuffer coordinates to world space
    Vector2 FramebufferToWorld(Vector2 fbPos);

    // Get normalized selection box (handles negative width/height)
    Rectangle GetSelectionBox() const;

    // Deselect all entities with SelectionComponent
    void DeselectAll();

    // Find entity under point, returns 0 if none
    Entity FindEntityAtPoint(Vector2 worldPos);
};

}  // namespace Elysium::Systems

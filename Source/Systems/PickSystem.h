#pragma once

#include "Core/Entity.h"
#include "Core/Event.h"
#include "Core/System.h"

namespace Elysium::Systems {

/**
 * PickSystem handles mouse picking and dragging of entities with BoundsComponent.
 * It listens to mouse events and updates entity positions during drag operations.
 */
class PickSystem : public System {
   public:
    PickSystem(Context context);

    void Update(float deltaTime) override;
    void OnEvent(Event& event);

   private:
    Entity draggedEntity_ = 0;
    bool isDragging_ = false;
    Vector2 dragOffset_ = {0, 0};  // Offset from entity position to mouse position

    void HandleMousePressed(const MouseButtonPressedEvent& event);
    void HandleMouseMoved(const MouseMovedEvent& event);
    void HandleMouseReleased(const MouseButtonReleasedEvent& event);

    // Check if a point is inside an entity's bounds
    bool IsPointInBounds(Vector2 point, const Rectangle& bounds);

    // Transform framebuffer coordinates to world space
    Vector2 FramebufferToWorld(Vector2 fbPos);
};

}  // namespace Elysium::Systems

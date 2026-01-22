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
    int button;  // Only for PRESS and RELEASE
};

/**
 * PickSystem handles mouse picking and dragging of entities with BoundsComponent.
 * It listens to mouse events and updates entity positions during drag operations.
 */
class PickSystem : public System {
   public:
    PickSystem(Context context);

    void Update(float deltaTime) override;
    void OnEvent(Event& event) override;

   private:
    void FireEvent(const PickEvent& pickEvent);

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

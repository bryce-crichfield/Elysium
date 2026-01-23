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
class PickSystem : public System, public IWorldListener, public IMouseListener {
   public:
    PickSystem(Context context);
    ~PickSystem();

    void Update(float deltaTime) override;

    // IEventListener - composes dispatch helpers
    void OnEvent(Event& event) override;

    // IWorldListener
    void OnEntityCreated(Entity entity) override {}
    void OnEntityDestroyed(Entity entity) override {}

    // IMouseListener overrides
    void OnMouseButtonPressed(MouseButtonPressedEvent& event) override;
    void OnMouseButtonReleased(MouseButtonReleasedEvent& event) override;
    void OnMouseMoved(MouseMovedEvent& event) override;

   private:
    Entity draggedEntity_ = 0;
    bool isDragging_ = false;
    Vector2 dragOffset_ = {0, 0};  // Offset from entity position to mouse position

    // Check if a point is inside an entity's bounds
    bool IsPointInBounds(Vector2 point, const Rectangle& bounds);

    // Transform framebuffer coordinates to world space
    Vector2 FramebufferToWorld(Vector2 fbPos);
};

}  // namespace Elysium::Systems

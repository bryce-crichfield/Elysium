#pragma once

#include "Core/Entity.h"
#include "Core/Event.h"
#include "Core/System.h"

namespace Elysium::Systems {

/**
 * CommandSystem handles issuing commands to selected units.
 * - Right-click on ground: issues move command to all selected units
 * - Can be extended for other command types (attack, patrol, etc.)
 */
class CommandSystem : public System, public IMouseListener {
   public:
    CommandSystem(Context context);
    ~CommandSystem() = default;

    void Update(float deltaTime) override;

    // IEventListener
    void OnEvent(Event& event) override;

    // IMouseListener overrides
    void OnMouseButtonPressed(MouseButtonPressedEvent& event) override;
    void OnMouseButtonReleased(MouseButtonReleasedEvent& event) override;
    void OnMouseMoved(MouseMovedEvent& event) override {}

   private:
    // Transform framebuffer coordinates to world space
    Vector2 FramebufferToWorld(Vector2 fbPos);

    // Issue move command to all selected entities
    void IssueMoveCommand(Vector2 targetPos);
};

}  // namespace Elysium::Systems

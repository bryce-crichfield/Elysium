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

    void OnEvent(Event& event) override;
    void OnMouseButtonPressed(MouseButtonPressedEvent& event) override;
    void OnMouseButtonReleased(MouseButtonReleasedEvent& event) override;

private:
    Vector2 FramebufferToWorld(Vector2 fbPos);
    void IssueMoveCommand(Vector2 targetPos);
    void IssueAttackCommand(Entity targetEntity);
    Entity FindEntityAtPoint(Vector2 worldPos);
};

}  // namespace Elysium::Systems

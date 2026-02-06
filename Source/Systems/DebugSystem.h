#pragma once

#include "Core/Entity.h"
#include "Core/Event.h"
#include "Core/System.h"
#include "Core/World.h"
#include "Components/RectangleComponent.h"
#include "Components/CircleComponent.h"
#include "Components/TextComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/LightComponent.h"
#include <variant>
#include <vector>

namespace Elysium::Systems {

using Debuggable = std::variant<
    RectangleComponent,
    CircleComponent,
    TextComponent,
    SpriteComponent,
    LightComponent
>;

class DebugSystem : public System, public IWorldListener, public IMouseListener {
   public:
    DebugSystem(Context context);
    ~DebugSystem();

    void Update(float deltaTime) override;
    void Draw() override;

    // IEventListener
    void OnEvent(Event& event) override;

    // IWorldListener
    void OnEntityCreated(Entity entity) override;
    void OnEntityDestroyed(Entity entity) override {}

    // IMouseListener
    void OnMouseButtonPressed(MouseButtonPressedEvent& event) override;
    void OnMouseButtonReleased(MouseButtonReleasedEvent& event) override;
    void OnMouseMoved(MouseMovedEvent& event) override;

   private:
    Entity selectedEntity_ = 0;
    bool isDragging_ = false;
    bool isPanning_ = false;

    Vector2 FramebufferToWorld(Vector2 fbPos);
    Entity FindEntityAtPoint(Vector2 fbPos);
    std::vector<Debuggable> GetDebuggables(Entity entity);
};

}  // namespace Elysium::Systems

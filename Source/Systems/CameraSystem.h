#pragma once

#include "Core/System.h"
#include "raylib.h"

namespace Elysium::Systems {

class CameraSystem : public System, IMouseListener, IKeyboardListener {
public:
    CameraSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;

    void OnEvent(Event& event) override;

    void OnMouseButtonPressed(MouseButtonPressedEvent& event) override;
    void OnMouseButtonReleased(MouseButtonReleasedEvent& event) override;
    void OnMouseMoved(MouseMovedEvent& event) override;
    void OnMouseWheel(MouseWheelEvent& event) override;
    void OnMouseEnter(MouseEnterEvent& event) override;
    void OnMouseExit(MouseExitEvent& event) override;

    void OnKeyPressed(KeyPressedEvent& event) override;
    void OnKeyReleased(KeyReleasedEvent& event) override;

private:
    Vector2 LerpVector2(Vector2 start, Vector2 end, float t);

    Vector2 mousePosition_{0.0f, 0.0f};
    Vector2 mouseDelta_{0.0f, 0.0f};
    bool isDragging_ = false;
    bool hasFocus_ = true;

    bool keyW_ = false;
    bool keyA_ = false;
    bool keyS_ = false;
    bool keyD_ = false;
};

}  // namespace Elysium::Systems

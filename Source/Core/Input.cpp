#include "Core/Input.h"

#include "raylib.h"
#include "Core/RaylibConvert.h"

namespace Elysium::Input {

bool IsKeyDown(Key key) { return ::IsKeyDown(static_cast<int>(key)); }
bool IsKeyPressed(Key key) { return ::IsKeyPressed(static_cast<int>(key)); }
bool IsKeyReleased(Key key) { return ::IsKeyReleased(static_cast<int>(key)); }

bool IsMouseButtonDown(MouseButton button) { return ::IsMouseButtonDown(static_cast<int>(button)); }
bool IsMouseButtonPressed(MouseButton button) { return ::IsMouseButtonPressed(static_cast<int>(button)); }
bool IsMouseButtonReleased(MouseButton button) { return ::IsMouseButtonReleased(static_cast<int>(button)); }

Vector2 GetMousePosition() { return FromRaylib(::GetMousePosition()); }
Vector2 GetMouseDelta() { return FromRaylib(::GetMouseDelta()); }
float GetMouseWheelMove() { return ::GetMouseWheelMove(); }

}  // namespace Elysium::Input

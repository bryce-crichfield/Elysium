#include "Core/Window.h"

#include "raylib.h"
#include "Core/RaylibConvert.h"

namespace Elysium {

Window::Window(int width, int height, const std::string& title) {
    ::SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    ::InitWindow(width, height, title.c_str());
    ::SetExitKey(0);  // App owns close handling (menu/scene scripts); disable raylib's default ESC-to-quit.
    open_ = true;
}

Window::~Window() { Destroy(); }

Window::Window(Window&& other) noexcept : open_(other.open_) {
    other.open_ = false;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        Destroy();
        open_ = other.open_;
        other.open_ = false;
    }
    return *this;
}

void Window::Destroy() {
    if (open_) {
        ::CloseWindow();
        open_ = false;
    }
}

bool Window::ShouldClose() const { return ::WindowShouldClose(); }

void Window::Maximize() { ::MaximizeWindow(); }

void Window::SetTitle(const std::string& title) { ::SetWindowTitle(title.c_str()); }

int Window::GetWidth() const { return ::GetScreenWidth(); }

int Window::GetHeight() const { return ::GetScreenHeight(); }

float Window::GetDeltaTime() const { return ::GetFrameTime(); }

void Window::BeginFrame(Color clearColor) {
    ::BeginDrawing();
    ::ClearBackground(ToRaylib(clearColor));
}

void Window::EndFrame() { ::EndDrawing(); }

}  // namespace Elysium

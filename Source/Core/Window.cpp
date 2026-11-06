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

bool Window::ShouldClose() const {
#ifdef __EMSCRIPTEN__
    // A page is never asked to close; the tab just goes. raylib's web WindowShouldClose also
    // calls emscripten_sleep, which aborts in a build without Asyncify.
    return false;
#else
    return ::WindowShouldClose();
#endif
}

void Window::Maximize() { ::MaximizeWindow(); }

void Window::SetTitle(const std::string& title) { ::SetWindowTitle(title.c_str()); }

int Window::GetWidth() const { return ::GetScreenWidth(); }

int Window::GetHeight() const { return ::GetScreenHeight(); }

float Window::GetDeltaTime() const { return ::GetFrameTime(); }

void Window::BeginFrame(Color clearColor) {
#ifdef __EMSCRIPTEN__
    // raylib resizes the canvas on a browser resize without telling Emscripten's GLFW, which
    // scales mouse positions by the window size it last heard of. Hand it the new one.
    static int glfwWidth = 0, glfwHeight = 0;
    if (::GetScreenWidth() != glfwWidth || ::GetScreenHeight() != glfwHeight) {
        glfwWidth = ::GetScreenWidth();
        glfwHeight = ::GetScreenHeight();
        ::SetWindowSize(glfwWidth, glfwHeight);
    }
#endif
    ::BeginDrawing();
    ::ClearBackground(ToRaylib(clearColor));
}

void Window::EndFrame() { ::EndDrawing(); }

}  // namespace Elysium

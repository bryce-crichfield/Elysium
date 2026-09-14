#pragma once

#include <string>

#include "Core/Graphics.h"

// The OS window + graphics-context lifecycle. RAII: raylib's InitWindow/CloseWindow
// today, GLFW's glfwCreateWindow/glfwDestroyWindow later. Implemented in Window.cpp
// so raylib.h stays out of every header, including Application.h. Move-only: the
// underlying window is a single process-global resource.

namespace Elysium {

class Window {
   public:
    Window() = default;
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    bool IsOpen() const { return open_; }
    bool ShouldClose() const;

    void Maximize();
    void SetTitle(const std::string& title);

    int GetWidth() const;
    int GetHeight() const;

    // Seconds elapsed since the previous EndFrame(), measured by the backend's own clock.
    float GetDeltaTime() const;

    // Frame bracket around a full draw: clears the backbuffer to clearColor, and on
    // EndFrame() presents (swaps buffers).
    void BeginFrame(Color clearColor);
    void EndFrame();

   private:
    void Destroy();

    bool open_ = false;
};

}  // namespace Elysium

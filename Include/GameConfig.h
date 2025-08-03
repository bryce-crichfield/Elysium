#pragma once

#include "raylib.h"
#include <string>

namespace Elysium {

struct GameConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle = "Elysium - 2D Game Engine";
    bool fullscreen = false;
    bool vsync = true;
    int targetFPS = 60;
    Color backgroundColor;
    
    int framebufferWidth = 640;
    int framebufferHeight = 480;
    
    float gravity = 9.81f;
    float defaultBallRadius = 20.0f;
    Vector2 defaultBallSpeed = { 5.0f, 4.0f };
    
    bool showDemoWindow = true;
    bool showMetrics = false;
    std::string logLevel = "INFO";
};

} // namespace Elysium
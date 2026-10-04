#pragma once

#include "Core/ServiceLocator.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Graphics.h"
#include "Core/Window.h"

namespace Elysium {

class EditorApplication;

enum class AppMode { Play, Editor };

// What the game draws at in Play. Default is native: the framebuffer is the window's own size.
// The rest draw at that fixed size and letterbox it onto the window.
enum class Resolution { Default, HD720, HD1080, QHD1440, UHD2160 };

// A fixed resolution's size; false for Default (native).
inline bool FixedResolution(Resolution resolution, int& width, int& height) {
    switch (resolution) {
        case Resolution::HD720:   width = 1280; height = 720;  return true;
        case Resolution::HD1080:  width = 1920; height = 1080; return true;
        case Resolution::QHD1440: width = 2560; height = 1440; return true;
        case Resolution::UHD2160: width = 3840; height = 2160; return true;
        default: return false;
    }
}

struct ApplicationConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle = "Elysium - 2D Game Engine";
    bool fullscreen = false;
    bool vsync = true;
    int targetFPS = 60;
    Color backgroundColor{0, 0, 0, 255};

    // The game screen: the size Screen2D (UI) layers and scripts lay things out in, whatever
    // the resolution. It's fitted (scaled, centered) into the framebuffer (ISceneService::ScreenFit),
    // and the game camera shows the same picture of the world at any resolution.
    int screenWidth = 640;
    int screenHeight = 480;
    Resolution resolution = Resolution::Default;

    bool showDemoWindow = true;
    bool showMetrics = false;
    std::string logLevel = "INFO";


    static bool FromXML(const std::string& path, ApplicationConfig& out);
};

// Application is owned by main.cpp as a plain local object — not a singleton.
// Nothing else should hold a reference to it; Systems/Components/Editors/
// Services all reach app-owned state (mode, config, clock) through
// IApplicationService via the ServiceLocator, same as every other service.
class Application {
   public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize(const std::string& configPath = "Config/ApplicationConfig.xml");
    void Run();
    void Shutdown();

    const ApplicationConfig& GetConfig() const { return config_; }

    ServiceLocator& GetServiceLocator() { return serviceLocator_; }

    bool ShouldClose() const;
    void RequestClose() { shouldClose_ = true; }

    void SetMode(AppMode mode);
    AppMode GetMode() const { return mode_; }

    // Get time in seconds since application start
    float GetTime() const { return startTime_; }

    int GetWindowWidth() const { return window_.GetWidth(); }
    int GetWindowHeight() const { return window_.GetHeight(); }

   private:
    void Update(float deltaTime);
    void Draw();
    void ProcessEvents();

    void ProcessInput();

    ApplicationConfig config_;
    Window window_;

    ServiceLocator serviceLocator_;
    std::unique_ptr<EditorApplication> editor_;  // the in-engine editor

    AppMode mode_ = AppMode::Play;

    bool initialized_ = false;
    bool shouldClose_ = false;

    float startTime_ = 0.0f;
};

}  // namespace Elysium

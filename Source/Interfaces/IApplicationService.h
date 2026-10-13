#pragma once

#include <string>
#include "Core/Graphics.h"
#include "Interfaces/IService.h"

namespace Elysium {

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

}  // namespace Elysium

namespace Elysium::Services {

// Uniform access to Application-owned state (window mode, config, clock) — the
// one exception to "everything is a real service": there is exactly one
// Application, so this just forwards to it. Exists so Systems/Components/
// Editors never need a raw Application* alongside their ServiceLocator.
class IApplicationService : public IService {
   public:

    virtual const ApplicationConfig& GetConfig() const = 0;
    virtual AppMode GetMode() const = 0;
    virtual void SetMode(AppMode mode) = 0;
    virtual float GetTime() const = 0;
    virtual bool ShouldClose() const = 0;
    // Ends the main loop after this frame: the app shuts down and exits.
    virtual void RequestClose() = 0;
    // Ends the main loop after this frame, then restarts the app on another project.
    virtual void RequestOpenProject(const std::string& projectXmlPath) = 0;
    virtual int GetWindowWidth() const = 0;
    virtual int GetWindowHeight() const = 0;
};

}  // namespace Elysium::Services

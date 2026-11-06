#pragma once

#include "Core/ServiceLocator.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IEditor.h"
#include "Core/Window.h"

namespace Elysium {

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

    // The services are registered by the caller before this; it initializes them.
    bool Initialize(const std::string& configPath = "Config/ApplicationConfig.xml");
    // Runs frames until the app closes. Returns the Project.xml to reopen the app on, or
    // nothing when it's quitting.
    std::optional<std::string> Run();
    // One frame, for hosts that own the loop (the browser). False once the app should close.
    bool Frame();
    void Shutdown();

    // Set after Initialize, which opens the window the editor draws into. Without one, Editor
    // mode is never entered.
    void SetEditor(std::unique_ptr<IEditor> editor);

    const ApplicationConfig& GetConfig() const { return config_; }

    ServiceLocator& GetServiceLocator() { return serviceLocator_; }

    bool ShouldClose() const;
    void RequestClose() { shouldClose_ = true; }
    void RequestOpenProject(const std::string& projectXmlPath);

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
    std::unique_ptr<IEditor> editor_;

    AppMode mode_ = AppMode::Play;

    bool initialized_ = false;
    bool shouldClose_ = false;
    std::optional<std::string> nextProject_;

    float startTime_ = 0.0f;
};

}  // namespace Elysium

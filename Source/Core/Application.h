#pragma once

#include "Core/ServiceLocator.h"
#include "Editor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Graphics.h"

namespace Elysium {

enum class AppMode { Play, Editor };

struct ApplicationConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle = "Elysium - 2D Game Engine";
    bool fullscreen = false;
    bool vsync = true;
    int targetFPS = 60;
    Color backgroundColor{0, 0, 0, 255};

    int framebufferWidth = 640;
    int framebufferHeight = 480;

    bool showDemoWindow = true;
    bool showMetrics = false;
    std::string logLevel = "INFO";

    std::string editorFontName = "";

    static bool FromXML(const std::string& path, ApplicationConfig& out);
};

// Application is owned by main.cpp as a plain local object — not a singleton.
// Nothing else should hold a reference to it; Systems/Components/Editors/
// Services all reach app-owned state (mode, config, clock) through
// IApplicationService via the ServiceLocator, same as every other service.
class Application {
   public:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize(const std::string& configPath = "Config/ApplicationConfig.xml");
    void Run();
    void Shutdown();

    const ApplicationConfig& GetConfig() const { return config_; }

    ServiceLocator& GetServiceLocator() { return serviceLocator_; }

    template <typename T, typename... Args>
    T& RegisterEditor(Args&&... args) {
        auto editor = std::make_unique<T>(serviceLocator_, std::forward<Args>(args)...);
        T& ref = *editor;
        editors_.push_back(std::move(editor));
        return ref;
    }

    template <typename T>
    T* GetEditor() {
        for (auto& editor : editors_) {
            if (auto* typed = dynamic_cast<T*>(editor.get())) {
                return typed;
            }
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<Editor>>& GetEditors() const { return editors_; }

    bool ShouldClose() const;

    void RequestFontReload() { pendingFontReload_ = true; }

    void SetMode(AppMode mode);
    AppMode GetMode() const { return mode_; }

    // Get time in seconds since application start
    float GetTime() const { return startTime_; }

   private:
    void Update(float deltaTime);
    void Draw();
    void DrawMenuBar();
    void ProcessEvents();

    void ProcessInput();

    ApplicationConfig config_;

    ServiceLocator serviceLocator_;
    std::vector<std::unique_ptr<Editor>> editors_;

    AppMode mode_ = AppMode::Play;

    bool initialized_ = false;
    bool shouldClose_ = false;
    bool pendingFontReload_ = false;
    bool editorLayoutBuilt_ = false;

    float startTime_ = 0.0f;
};

}  // namespace Elysium

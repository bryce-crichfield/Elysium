#pragma once

#include "Services/Service.h"
#include "Editor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "raylib.h"

namespace Elysium {

struct ApplicationConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle = "Elysium - 2D Game Engine";
    bool fullscreen = false;
    bool vsync = true;
    int targetFPS = 60;
    Color backgroundColor = BLACK;

    int framebufferWidth = 640;
    int framebufferHeight = 480;

    bool showDemoWindow = true;
    bool showMetrics = false;
    std::string logLevel = "INFO";

    static bool FromXML(const std::string& path, ApplicationConfig& out);
};

class Application {
   public:
    static Application& GetInstance();

    bool Initialize(const std::string& configPath = "Config/ApplicationConfig.xml");
    void Run();
    void Shutdown();

    const ApplicationConfig& GetConfig() const { return config_; }

    template <typename T>
    T& GetService() {
        return serviceRegistry_.GetService<T>();
    }

    template <typename T>
    void RegisterService(std::unique_ptr<T> service) {
        serviceRegistry_.RegisterService(std::move(service));
    }

    template <typename T, typename... Args>
    T& RegisterEditor(Args&&... args) {
        auto editor = std::make_unique<T>(std::forward<Args>(args)...);
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

   private:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Update(float deltaTime);
    void Draw();
    void ProcessEvents();

    void ProcessInput();

    ApplicationConfig config_;

    ServiceRegistry serviceRegistry_;
    std::vector<std::unique_ptr<Editor>> editors_;

    bool initialized_ = false;
    bool shouldClose_ = false;
    bool pendingFontReload_ = false;
};

}  // namespace Elysium

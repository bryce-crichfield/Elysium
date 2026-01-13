#pragma once

#include "Service.h"

#include "raylib.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Elysium {

struct ApplicationConfig {
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

    bool ShouldClose() const;
private:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Update(float deltaTime);
    void Draw();
    void ProcessEvents();

    void ProcessInput();
    void CalculateLetterboxing();
    Vector2 MapScreenToFramebuffer(Vector2 screenPos) const;

    ApplicationConfig config_;

    ServiceRegistry serviceRegistry_;

    RenderTexture2D frontBuffer_;
    RenderTexture2D backBuffer_;
    RenderTexture2D sceneFramebuffer_;
    RenderTexture2D transitionBuffer_;  

    Rectangle letterboxRect_;
    float scaleX_, scaleY_;
    Vector2 offset_;

    bool initialized_ = false;
    bool shouldClose_ = false;
};

} // namespace Elysium

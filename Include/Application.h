#pragma once

#include "Scene.h"
#include <unordered_map>
#include "Services/LogService.h"
#include "Services/EventService.h"
#include "Services/AssetService.h"
#include "Services/NetworkService.h"
#include "Services/LoadingService.h"
#include "Services/JukeboxService.h"
#include "Services/SceneService.h"
#include "raylib.h"
#include <memory>
#include <string>

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

    bool Initialize(const std::string& configPath = "./Assets/Config/ApplicationConfig.xml");
    void Run();
    void Shutdown();

    void SetScene(std::unique_ptr<Scene> scene);
    void QueueScene(const std::string& xmlPath);
    void DefineScene(const std::string& typeName, SceneFactory factory);
    void QueueScene(std::unique_ptr<Scene> scene);
    // Scene factory registration and XML loading
    const ApplicationConfig& GetConfig() const { return config_; }
    Services::SceneService& GetSceneService() { return sceneService_; }
    Services::EventService& GetEventService() { return eventService_; }
    Services::AssetService& GetAssetService() { return assetService_; }
    Services::NetworkService& GetNetworkService() { return networkService_; }
    Services::LogService& GetLogService() { return logService_; }
    Services::JukeboxService& GetJukeboxService() { return jukeboxService_; }

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

    Services::SceneService sceneService_;
    Services::LoadingService loadingService_;
    Services::EventService eventService_;
    Services::AssetService assetService_;
    Services::NetworkService networkService_;
    Services::LogService logService_;
    Services::JukeboxService jukeboxService_;

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

#pragma once

#include "Scene.h"
#include <unordered_map>
#include "Services/MetricsService.h"
#include "Services/LogService.h"
#include "Services/EventService.h"
#include "Services/AssetService.h"
#include "Services/NetworkService.h"
#include "GameConfig.h"
#include "raylib.h"
#include <memory>
#include <string>

namespace Elysium {

class Application {
public:
    static Application& GetInstance();
    
    bool Initialize(const std::string& configPath = "./Assets/GameConfig.xml");
    void Run();
    void Shutdown();
    
    void SetScene(std::unique_ptr<Scene> scene);
    void QueueSceneTransition(std::unique_ptr<Scene> scene);
    
    // Scene factory registration and XML loading
    void DefineScene(const std::string& typeName, SceneFactory factory);
    void QueueScene(const std::string& xmlPath);
    
    Services::EventService& GetEventService() { return eventService_; }
    Services::AssetService& GetAssetService() { return assetService_; }
    Services::NetworkService& GetNetworkService() { return networkService_; }
    Services::MetricsService& GetMetricsService() { return metricsService_; }
    Services::LogService& GetLogService() { return logService_; }
    const GameConfig& GetConfig() const { return config_; }
    
    bool ShouldClose() const;

private:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    void Update(float deltaTime);
    void Draw();
    void ProcessEvents();
    void HandleSceneTransition();
    
    GameConfig LoadGameConfig(const std::string& configPath);
    void ProcessInput();
    void CalculateLetterboxing();
    Vector2 MapScreenToFramebuffer(Vector2 screenPos) const;
    
    GameConfig config_;
    std::unique_ptr<Scene> currentScene_;
    std::unique_ptr<Scene> pendingScene_;
    bool sceneTransitionPending_ = false;
    bool sceneTransitionLocked_ = false;
    
    // Scene factory registry
    std::unordered_map<std::string, SceneFactory> sceneFactories_;
    
    bool inTransition_ = false;
    float transitionTimer_ = 0.0f;
    float transitionDuration_ = 1.0f;
    
    Services::EventService eventService_;
    Services::AssetService assetService_;
    Services::NetworkService networkService_;
    Services::MetricsService metricsService_;
    Services::LogService logService_;
    
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
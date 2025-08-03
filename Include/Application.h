#pragma once

#include "Scene.h"
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
    
    Services::EventService& GetEventService() { return eventService_; }
    Services::AssetService& GetAssetService() { return assetService_; }
    Services::NetworkService& GetNetworkService() { return networkService_; }
    Services::MetricsService& GetMetricsService() { return metricsService_; }
    Services::LogService& GetLogService() { return Services::LogService::GetInstance(); }
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
    
    GameConfig config_;
    std::unique_ptr<Scene> currentScene_;
    std::unique_ptr<Scene> pendingScene_;
    bool sceneTransitionPending_ = false;
    bool sceneTransitionLocked_ = false;
    
    Services::EventService eventService_;
    Services::AssetService assetService_;
    Services::NetworkService networkService_;
    Services::MetricsService metricsService_;
    
    RenderTexture2D frontBuffer_;
    RenderTexture2D backBuffer_;
    
    bool initialized_ = false;
    bool shouldClose_ = false;
};

} // namespace Elysium
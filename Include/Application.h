#pragma once

#include "IScene.h"
#include "MetricsService.h"
#include "LogService.h"
#include "raylib.h"
#include <memory>
#include <string>
#include <queue>
#include <vector>

struct GameConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle = "Elysium - 2D Game Engine";
    bool fullscreen = false;
    bool vsync = true;
    int targetFPS = 60;
    
    Color backgroundColor = DARKBLUE;
    bool showFPS = true;
    
    float gravity = 9.81f;
    float defaultBallRadius = 20.0f;
    Vector2 defaultBallSpeed = { 5.0f, 4.0f };
    
    bool showDemoWindow = true;
    bool showMetrics = false;
    std::string logLevel = "INFO";
};

class EventManager {
public:
    void QueueInputEvent(const InputEvent& event);
    void QueueNetworkEvent(const NetworkEvent& event);
    
    bool HasInputEvents() const;
    bool HasNetworkEvents() const;
    
    InputEvent GetNextInputEvent();
    NetworkEvent GetNextNetworkEvent();
    
    void Clear();

private:
    std::queue<InputEvent> inputEvents_;
    std::queue<NetworkEvent> networkEvents_;
};

class AssetManager {
public:
    void Initialize();
    void Shutdown();
    
    Texture2D LoadTexture(const std::string& path);
    Sound LoadSound(const std::string& path);
    void UnloadTexture(const std::string& path);
    void UnloadSound(const std::string& path);
};

class NetworkManager {
public:
    void Initialize();
    void Shutdown();
    void Update();
    
    bool StartServer(int port);
    bool ConnectToServer(const std::string& address, int port);
    void Disconnect();
    
    void SendData(const void* data, size_t size);
    bool IsConnected() const;

private:
    bool isServer_ = false;
    bool isConnected_ = false;
};

class Application {
public:
    static Application& GetInstance();
    
    bool Initialize(const std::string& configPath = "./Assets/GameConfig.xml");
    void Run();
    void Shutdown();
    
    void SetScene(std::unique_ptr<IScene> scene);
    void QueueSceneTransition(std::unique_ptr<IScene> scene);
    
    EventManager& GetEventManager() { return eventManager_; }
    AssetManager& GetAssetManager() { return assetManager_; }
    NetworkManager& GetNetworkManager() { return networkManager_; }
    MetricsService& GetMetricsService() { return metricsService_; }
    LogService& GetLogService() { return LogService::GetInstance(); }
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
    std::unique_ptr<IScene> currentScene_;
    std::unique_ptr<IScene> pendingScene_;
    bool sceneTransitionPending_ = false;
    bool sceneTransitionLocked_ = false;
    
    EventManager eventManager_;
    AssetManager assetManager_;
    NetworkManager networkManager_;
    MetricsService metricsService_;
    
    RenderTexture2D frontBuffer_;
    RenderTexture2D backBuffer_;
    
    bool initialized_ = false;
    bool shouldClose_ = false;
};
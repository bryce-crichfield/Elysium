#include "Application.h"
#include "rlImGui.h"
#include "imgui.h"
#include "tinyxml2.h"
#include <cstdio>
#include <cstdarg>
#include <unordered_map>

using namespace tinyxml2;

void EventManager::QueueInputEvent(const InputEvent& event) {
    inputEvents_.push(event);
}

void EventManager::QueueNetworkEvent(const NetworkEvent& event) {
    networkEvents_.push(event);
}

bool EventManager::HasInputEvents() const {
    return !inputEvents_.empty();
}

bool EventManager::HasNetworkEvents() const {
    return !networkEvents_.empty();
}

InputEvent EventManager::GetNextInputEvent() {
    if (inputEvents_.empty()) {
        return {};
    }
    InputEvent event = inputEvents_.front();
    inputEvents_.pop();
    return event;
}

NetworkEvent EventManager::GetNextNetworkEvent() {
    if (networkEvents_.empty()) {
        return {};
    }
    NetworkEvent event = networkEvents_.front();
    networkEvents_.pop();
    return event;
}

void EventManager::Clear() {
    while (!inputEvents_.empty()) inputEvents_.pop();
    while (!networkEvents_.empty()) networkEvents_.pop();
}

static std::unordered_map<std::string, Texture2D> textureCache;
static std::unordered_map<std::string, Sound> soundCache;

void AssetManager::Initialize() {
    textureCache.clear();
    soundCache.clear();
}

void AssetManager::Shutdown() {
    for (auto& pair : textureCache) {
        ::UnloadTexture(pair.second);
    }
    for (auto& pair : soundCache) {
        ::UnloadSound(pair.second);
    }
    textureCache.clear();
    soundCache.clear();
}

Texture2D AssetManager::LoadTexture(const std::string& path) {
    auto it = textureCache.find(path);
    if (it != textureCache.end()) {
        return it->second;
    }
    
    Texture2D texture = ::LoadTexture(path.c_str());
    textureCache[path] = texture;
    return texture;
}

Sound AssetManager::LoadSound(const std::string& path) {
    auto it = soundCache.find(path);
    if (it != soundCache.end()) {
        return it->second;
    }
    
    Sound sound = ::LoadSound(path.c_str());
    soundCache[path] = sound;
    return sound;
}

void AssetManager::UnloadTexture(const std::string& path) {
    auto it = textureCache.find(path);
    if (it != textureCache.end()) {
        ::UnloadTexture(it->second);
        textureCache.erase(it);
    }
}

void AssetManager::UnloadSound(const std::string& path) {
    auto it = soundCache.find(path);
    if (it != soundCache.end()) {
        ::UnloadSound(it->second);
        soundCache.erase(it);
    }
}

void NetworkManager::Initialize() {
    isServer_ = false;
    isConnected_ = false;
}

void NetworkManager::Shutdown() {
    if (isConnected_) {
        Disconnect();
    }
}

void NetworkManager::Update() {
}

bool NetworkManager::StartServer(int port) {
    return false;
}

bool NetworkManager::ConnectToServer(const std::string& address, int port) {
    return false;
}

void NetworkManager::Disconnect() {
    isConnected_ = false;
    isServer_ = false;
}

void NetworkManager::SendData(const void* data, size_t size) {
}

bool NetworkManager::IsConnected() const {
    return isConnected_;
}

Application& Application::GetInstance() {
    static Application instance;
    return instance;
}

void CustomTraceLogCallback(int logLevel, const char *text, va_list args) {
    const char* levelColors[] = {
        "\033[37m", "\033[37m", "\033[37m", "\033[37m",
        "\033[93m", "\033[91m", "\033[95m", "\033[90m"
    };
    
    const char* levelNames[] = {
        "ALL", "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL", "NONE"
    };
    
    const char* resetColor = "\033[0m";
    
    int safeLevel = (logLevel >= 0 && logLevel < 8) ? logLevel : 3;
    
    printf("%s[%s] ", levelColors[safeLevel], levelNames[safeLevel]);
    vprintf(text, args);
    printf("%s\n", resetColor);
}

bool Application::Initialize(const std::string& configPath) {
    if (initialized_) {
        return true;
    }
    
    SetTraceLogCallback(CustomTraceLogCallback);
    SetTraceLogLevel(LOG_INFO);
    
    config_ = LoadGameConfig(configPath);
    
    TraceLog(LOG_INFO, "Elysium Engine initializing...");
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(config_.windowWidth, config_.windowHeight, config_.windowTitle.c_str());
    
    rlImGuiSetup(true);
    SetTargetFPS(config_.targetFPS);
    
    frontBuffer_ = LoadRenderTexture(config_.windowWidth, config_.windowHeight);
    backBuffer_ = LoadRenderTexture(config_.windowWidth, config_.windowHeight);
    
    assetManager_.Initialize();
    networkManager_.Initialize();
    
    initialized_ = true;
    return true;
}

void Application::Run() {
    if (!initialized_) {
        TraceLog(LOG_ERROR, "Application not initialized!");
        return;
    }
    
    while (!WindowShouldClose() && !shouldClose_) {
        float deltaTime = GetFrameTime();
        
        ProcessInput();
        ProcessEvents();
        HandleSceneTransition();
        Update(deltaTime);
        Draw();
    }
}

void Application::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    if (currentScene_) {
        currentScene_->OnExit();
    }
    
    UnloadRenderTexture(frontBuffer_);
    UnloadRenderTexture(backBuffer_);
    
    assetManager_.Shutdown();
    networkManager_.Shutdown();
    
    rlImGuiShutdown();
    CloseWindow();
    
    initialized_ = false;
}

void Application::SetScene(std::unique_ptr<IScene> scene) {
    if (currentScene_) {
        currentScene_->OnExit();
    }
    currentScene_ = std::move(scene);
    if (currentScene_) {
        currentScene_->OnEnter();
    }
}

void Application::QueueSceneTransition(std::unique_ptr<IScene> scene) {
    if (!sceneTransitionLocked_) {
        pendingScene_ = std::move(scene);
        sceneTransitionPending_ = true;
    }
}

bool Application::ShouldClose() const {
    return shouldClose_ || WindowShouldClose();
}

void Application::Update(float deltaTime) {
    if (currentScene_) {
        currentScene_->OnUpdate(deltaTime);
    }
    
    networkManager_.Update();
}

void Application::Draw() {
    BeginDrawing();
    ClearBackground(config_.backgroundColor);
    
    rlImGuiBegin();
    
    if (currentScene_) {
        currentScene_->OnDraw();
    }
    
    rlImGuiEnd();
    
    if (config_.showFPS) {
        DrawFPS(GetScreenWidth() - 95, 10);
    }
    
    EndDrawing();
}

void Application::ProcessEvents() {
    while (eventManager_.HasInputEvents()) {
        InputEvent event = eventManager_.GetNextInputEvent();
        if (currentScene_) {
            currentScene_->OnInput(event);
        }
    }
    
    while (eventManager_.HasNetworkEvents()) {
        NetworkEvent event = eventManager_.GetNextNetworkEvent();
        if (currentScene_) {
            currentScene_->OnNetwork(event);
        }
    }
}

void Application::HandleSceneTransition() {
    if (sceneTransitionPending_ && !sceneTransitionLocked_) {
        sceneTransitionLocked_ = true;
        SetScene(std::move(pendingScene_));
        sceneTransitionPending_ = false;
        sceneTransitionLocked_ = false;
    }
}

void Application::ProcessInput() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        InputEvent event;
        event.type = InputEvent::KEY_PRESS;
        event.key = KEY_ESCAPE;
        eventManager_.QueueInputEvent(event);
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        InputEvent event;
        event.type = InputEvent::MOUSE_PRESS;
        event.key = MOUSE_LEFT_BUTTON;
        Vector2 mousePos = GetMousePosition();
        event.x = mousePos.x;
        event.y = mousePos.y;
        eventManager_.QueueInputEvent(event);
    }
}

GameConfig Application::LoadGameConfig(const std::string& configPath) {
    GameConfig config;
    XMLDocument doc;
    
    if (doc.LoadFile(configPath.c_str()) != XML_SUCCESS) {
        TraceLog(LOG_ERROR, "Failed to load config file: %s. Using defaults.", configPath.c_str());
        return config;
    }
    
    XMLElement* root = doc.FirstChildElement("GameConfig");
    if (!root) {
        TraceLog(LOG_ERROR, "Invalid config file format. Using defaults.");
        return config;
    }
    
    if (XMLElement* window = root->FirstChildElement("Window")) {
        if (XMLElement* width = window->FirstChildElement("Width"))
            config.windowWidth = width->IntText(1280);
        if (XMLElement* height = window->FirstChildElement("Height"))
            config.windowHeight = height->IntText(720);
        if (XMLElement* title = window->FirstChildElement("Title"))
            config.windowTitle = title->GetText() ? title->GetText() : "Elysium";
        if (XMLElement* fullscreen = window->FirstChildElement("Fullscreen"))
            config.fullscreen = fullscreen->BoolText(false);
        if (XMLElement* vsync = window->FirstChildElement("VSync"))
            config.vsync = vsync->BoolText(true);
        if (XMLElement* fps = window->FirstChildElement("TargetFPS"))
            config.targetFPS = fps->IntText(60);
    }
    
    if (XMLElement* graphics = root->FirstChildElement("Graphics")) {
        if (XMLElement* bgColor = graphics->FirstChildElement("BackgroundColor")) {
            int r = bgColor->FirstChildElement("R") ? bgColor->FirstChildElement("R")->IntText(47) : 47;
            int g = bgColor->FirstChildElement("G") ? bgColor->FirstChildElement("G")->IntText(79) : 79;
            int b = bgColor->FirstChildElement("B") ? bgColor->FirstChildElement("B")->IntText(79) : 79;
            int a = bgColor->FirstChildElement("A") ? bgColor->FirstChildElement("A")->IntText(255) : 255;
            config.backgroundColor = (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
        }
        if (XMLElement* showFPS = graphics->FirstChildElement("ShowFPS"))
            config.showFPS = showFPS->BoolText(true);
    }
    
    if (XMLElement* physics = root->FirstChildElement("Physics")) {
        if (XMLElement* gravity = physics->FirstChildElement("Gravity"))
            config.gravity = gravity->FloatText(9.81f);
        if (XMLElement* ballRadius = physics->FirstChildElement("DefaultBallRadius"))
            config.defaultBallRadius = ballRadius->FloatText(20.0f);
        if (XMLElement* ballSpeed = physics->FirstChildElement("DefaultBallSpeed")) {
            float x = ballSpeed->FirstChildElement("X") ? ballSpeed->FirstChildElement("X")->FloatText(5.0f) : 5.0f;
            float y = ballSpeed->FirstChildElement("Y") ? ballSpeed->FirstChildElement("Y")->FloatText(4.0f) : 4.0f;
            config.defaultBallSpeed = (Vector2){ x, y };
        }
    }
    
    if (XMLElement* debug = root->FirstChildElement("Debug")) {
        if (XMLElement* showDemo = debug->FirstChildElement("ShowDemoWindow"))
            config.showDemoWindow = showDemo->BoolText(true);
        if (XMLElement* showMetrics = debug->FirstChildElement("ShowMetrics"))
            config.showMetrics = showMetrics->BoolText(false);
        if (XMLElement* logLevel = debug->FirstChildElement("LogLevel"))
            config.logLevel = logLevel->GetText() ? logLevel->GetText() : "INFO";
    }
    
    TraceLog(LOG_INFO, "Loaded game config from: %s", configPath.c_str());
    return config;
}
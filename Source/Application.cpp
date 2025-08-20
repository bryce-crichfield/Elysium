#include "Application.h"
#include "Services/LogService.h"
#include "rlImGui.h"
#include "imgui.h"
#include "tinyxml2.h"
#include <cstdio>
#include <cstdarg>
#include <chrono>
#include "Services/JukeboxService.h"
#include "Services/InspectorService.h"

using namespace tinyxml2;

namespace Elysium {

Application& Application::GetInstance() {
    static Application instance;
    return instance;
}

static Application* g_appInstance = nullptr;

void CustomTraceLogCallback(int logLevel, const char* text, va_list args) {
    if (g_appInstance) {
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), text, args);
        g_appInstance->GetLogService().LogMessage(logLevel, std::string(buffer));
    }
}

bool Application::Initialize(const std::string& configPath) {
    if (initialized_) {
        return true;
    }

        g_appInstance = this;
        SetTraceLogCallback(CustomTraceLogCallback);
        SetTraceLogLevel(LOG_DEBUG);

        if (!ApplicationConfig::FromXML(configPath, config_)) {
            LOG_ERROR("Application", "Failed to load ApplicationConfig.xml");
            return false;
        }

        LOG_INFO("Application", "Elysium Engine initializing");

        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(config_.windowWidth, config_.windowHeight, config_.windowTitle.c_str());

        InitAudioDevice();

        rlImGuiSetup(true);
        // SetTargetFPS(config_.targetFPS);

        frontBuffer_ = LoadRenderTexture(config_.windowWidth, config_.windowHeight);
        backBuffer_ = LoadRenderTexture(config_.windowWidth, config_.windowHeight);
        sceneFramebuffer_ = LoadRenderTexture(config_.framebufferWidth, config_.framebufferHeight);
        transitionBuffer_ = LoadRenderTexture(config_.framebufferWidth, config_.framebufferHeight);

        CalculateLetterboxing();

        assetService_.Initialize();
        networkService_.Initialize();
        logService_.Initialize();
        persistenceService_.Initialize();
        inspectorService_.Initialize();
        // loadingService_.Initialize(); // Temporarily disabled for testing Scene.xml loading

        initialized_ = true;
        LOG_INFO("Application", "Engine initialization complete");
        return true;
    }

void Application::Run() {
    if (!initialized_) {
        TraceLog(LOG_ERROR, "Application not initialized!");
        return;
    }

    while (!WindowShouldClose() && !shouldClose_) {
        float deltaTime = GetFrameTime();

        if (IsWindowResized()) {
            CalculateLetterboxing();
        }

            ProcessInput();
            ProcessEvents();
            Update(deltaTime);
            Draw();
        }
        Shutdown();
    }

    void Application::Shutdown()
    {
        if (!initialized_)
        {
            return;
        }

        logService_.Shutdown();
        g_appInstance = nullptr;

        sceneService_.Shutdown();

        UnloadRenderTexture(frontBuffer_);
        UnloadRenderTexture(backBuffer_);
        UnloadRenderTexture(sceneFramebuffer_);
        UnloadRenderTexture(transitionBuffer_);

        loadingService_.Shutdown();
        assetService_.Shutdown();
        networkService_.Shutdown();
        jukeboxService_.Stop();

        rlImGuiShutdown();
        CloseAudioDevice();
        CloseWindow();

        initialized_ = false;
    }

    void Application::SetScene(std::unique_ptr<Scene> scene)
    {
        sceneService_.SetScene(std::move(scene));
    }

    void Application::QueueScene(std::unique_ptr<Scene> scene)
    {
        sceneService_.QueueScene(std::move(scene));
    }

    void Application::DefineScene(const std::string& typeName, SceneFactory factory)
    {
        sceneService_.RegisterScene(typeName, factory);
    }

    void Application::QueueScene(const std::string& xmlPath)
    {
        sceneService_.QueueScene(xmlPath);
    }

    bool Application::ShouldClose() const
    {
        return shouldClose_ || WindowShouldClose();
    }

    void Application::Update(float deltaTime)
    {
        logService_.Update(deltaTime);

        jukeboxService_.Update();

        // Update scene service (handles transitions and current scene updates)
        sceneService_.Update(deltaTime);

        // Handle asset loading during transitions
        static bool loadingStarted = false;

        if (sceneService_.GetTransitionState() == Services::SceneService::TransitionState::LOADING && !loadingStarted) {
            // Start loading assets when we enter LOADING state
            const auto& pendingAssets = sceneService_.GetPendingAssets();
            if (!pendingAssets.empty()) {
                loadingService_.LoadAssets(pendingAssets, assetService_);
            }
            loadingStarted = true;
        }
        else if (sceneService_.GetTransitionState() == Services::SceneService::TransitionState::LOADING) {
            // Check if we're done loading
            if (!loadingService_.IsLoading())
            {
                // Loading complete, finalize GPU resources on main thread
                assetService_.FinalizeAssets();

                // Signal scene service that loading is complete
                sceneService_.OnAssetsLoaded();
            }
        }
        else if (sceneService_.GetTransitionState() == Services::SceneService::TransitionState::NONE) {
            loadingStarted = false;
        }

        inspectorService_.Update(deltaTime);

        networkService_.Update();
    }

    void Application::Draw()
    {
        auto renderStart = std::chrono::high_resolution_clock::now();
        auto screenRect = Rectangle { 0, 0, config_.framebufferWidth, config_.framebufferHeight};
        Scene* currentScene = sceneService_.GetScene();

        if (sceneService_.IsTransitioning())
        {
            if (sceneService_.GetTransitionState() == Services::SceneService::TransitionState::LOADING)
            {
                BeginDrawing();
                ClearBackground(BLACK);
                int screenWidth = GetScreenWidth();
                int screenHeight = GetScreenHeight();

                // Delegate drawing to LoadingService
                loadingService_.Draw(screenWidth, screenHeight);
            }
            else
            {
                // Render current scene to framebuffer
                BeginTextureMode(sceneFramebuffer_);
                ClearBackground(config_.backgroundColor);
                if (currentScene)
                {
                    currentScene->OnDraw(screenRect);
                }
                EndTextureMode();

                BeginDrawing();
                ClearBackground(BLACK);

                float alpha = sceneService_.GetTransitionProgress();

                if (sceneService_.GetTransitionState() == Services::SceneService::TransitionState::FADE_OUT)
                {
                    Color currentTint = {255, 255, 255, (unsigned char)(255 * (1.0f - alpha))};
                    DrawTexturePro(
                        sceneFramebuffer_.texture,
                        Rectangle{0, 0, (float)sceneFramebuffer_.texture.width, -(float)sceneFramebuffer_.texture.height},
                        letterboxRect_,
                        Vector2{0, 0},
                        0.0f,
                        currentTint);
                }
                else if (sceneService_.GetTransitionState() == Services::SceneService::TransitionState::FADE_IN)
                {
                    Color pendingTint = {255, 255, 255, (unsigned char)(255 * alpha)};
                    DrawTexturePro(
                        sceneFramebuffer_.texture,
                        Rectangle{0, 0, (float)sceneFramebuffer_.texture.width, -(float)sceneFramebuffer_.texture.height},
                        letterboxRect_,
                        Vector2{0, 0},
                        0.0f,
                        pendingTint);
                }
            }
        }
        else
        {
            BeginTextureMode(sceneFramebuffer_);
            ClearBackground(config_.backgroundColor);
            if (currentScene)
            {
                currentScene->OnDraw(screenRect);
            }
            EndTextureMode();

            BeginDrawing();
            ClearBackground(BLACK);

            DrawTexturePro(
                sceneFramebuffer_.texture,
                Rectangle{0, 0, (float)sceneFramebuffer_.texture.width, -(float)sceneFramebuffer_.texture.height},
                letterboxRect_,
                Vector2{0, 0},
                0.0f,
                WHITE);
        }

        rlImGuiBegin();

        if (currentScene)
        {
            currentScene->OnDebugDraw();
        }

        logService_.Draw();
        persistenceService_.Draw();
        jukeboxService_.OnDebugDraw();
        
        // Set current world for inspector service
        if (currentScene) {
            inspectorService_.SetCurrentWorld(currentScene->GetWorld());
        }
        inspectorService_.Draw();


        rlImGuiEnd();

        EndDrawing();

        auto renderEnd = std::chrono::high_resolution_clock::now();
        float renderTime = std::chrono::duration<float>(renderEnd - renderStart).count();
    }

    void Application::ProcessEvents()
    {

    }


    void Application::ProcessInput()
    {
        ImGuiIO& io = ImGui::GetIO();

        if (IsKeyPressed(KEY_F2))
        {
        }

        if (IsKeyPressed(KEY_F3))
        {
            logService_.ToggleVisibility();
        }

        if (IsKeyPressed(KEY_F8))
        {
            persistenceService_.ToggleVisibility();
        }

        if (IsKeyPressed(KEY_F1))
        {
            // Toggle inspector system visibility in current scene
            Scene* currentScene = sceneService_.GetScene();
            if (currentScene)
            {
                inspectorService_.ToggleVisibility();
            }
        }
    }

    bool ApplicationConfig::FromXML(const std::string &configPath, ApplicationConfig& out)
    {
        ApplicationConfig& config = out;
        XMLDocument doc;

        if (doc.LoadFile(configPath.c_str()) != XML_SUCCESS)
        {
            TraceLog(LOG_ERROR, "Failed to load config file: %s. Using defaults.", configPath.c_str());
            return false;
        }

        XMLElement *root = doc.FirstChildElement("GameConfig");
        if (!root)
        {
            TraceLog(LOG_ERROR, "Invalid config file format. Using defaults.");
            return false;
        }

        if (XMLElement *window = root->FirstChildElement("Window"))
        {
            if (XMLElement *width = window->FirstChildElement("Width"))
                config.windowWidth = width->IntText(1280);
            if (XMLElement *height = window->FirstChildElement("Height"))
                config.windowHeight = height->IntText(720);
            if (XMLElement *title = window->FirstChildElement("Title"))
                config.windowTitle = title->GetText() ? title->GetText() : "Elysium";
            if (XMLElement *fullscreen = window->FirstChildElement("Fullscreen"))
                config.fullscreen = fullscreen->BoolText(false);
            if (XMLElement *vsync = window->FirstChildElement("VSync"))
                config.vsync = vsync->BoolText(true);
            if (XMLElement *fps = window->FirstChildElement("TargetFPS"))
                config.targetFPS = fps->IntText(60);
            if (XMLElement *backgroundColor = window->FirstChildElement("BackgroundColor"))
            {
                config.backgroundColor = BLACK;
                if (XMLElement *r = backgroundColor->FirstChildElement("r"))
                    config.backgroundColor.r = r->IntText(255);
                if (XMLElement *g = backgroundColor->FirstChildElement("g"))
                    config.backgroundColor.g = g->IntText(255);
                if (XMLElement *b = backgroundColor->FirstChildElement("b"))
                    config.backgroundColor.b = b->IntText(255);

                TraceLog(LOG_INFO, "Color: %d, %d, %d", config.backgroundColor.r, config.backgroundColor.g, config.backgroundColor.b);
            }
            if (XMLElement *framebuffer = window->FirstChildElement("Framebuffer"))
            {
                if (XMLElement *width = framebuffer->FirstChildElement("Width"))
                    config.framebufferWidth = width->IntText(640);
                if (XMLElement *height = framebuffer->FirstChildElement("Height"))
                    config.framebufferHeight = height->IntText(480);
            }
        }

        if (XMLElement *debug = root->FirstChildElement("Debug"))
        {
            if (XMLElement *showDemo = debug->FirstChildElement("ShowDemoWindow"))
                config.showDemoWindow = showDemo->BoolText(true);
            if (XMLElement *showMetrics = debug->FirstChildElement("ShowMetrics"))
                config.showMetrics = showMetrics->BoolText(false);
            if (XMLElement *logLevel = debug->FirstChildElement("LogLevel"))
                config.logLevel = logLevel->GetText() ? logLevel->GetText() : "INFO";
        }

        TraceLog(LOG_INFO, "Loaded game config from: %s", configPath.c_str());

        return true;
    }

    void Application::CalculateLetterboxing()
    {
        int windowWidth = GetScreenWidth();
        int windowHeight = GetScreenHeight();


        float screenAspect = (float)windowWidth / windowHeight;
        float framebufferAspect = (float)config_.framebufferWidth / config_.framebufferHeight;

        if (framebufferAspect > screenAspect)
        {
            scaleX_ = scaleY_ = (float)windowWidth / config_.framebufferWidth;
            float scaledHeight = config_.framebufferHeight * scaleY_;
            offset_.x = 0;
            offset_.y = (windowHeight - scaledHeight) * 0.5f;
            letterboxRect_ = Rectangle{offset_.x, offset_.y, (float)windowWidth, scaledHeight};
        }
        else
        {
            scaleX_ = scaleY_ = (float)windowHeight / config_.framebufferHeight;
            float scaledWidth = config_.framebufferWidth * scaleX_;
            offset_.x = (windowWidth - scaledWidth) * 0.5f;
            offset_.y = 0;
            letterboxRect_ = Rectangle{offset_.x, offset_.y, scaledWidth, (float)windowHeight};
        }
    }

    Vector2 Application::MapScreenToFramebuffer(Vector2 screenPos) const
    {
        Vector2 framebufferPos;
        framebufferPos.x = (screenPos.x - offset_.x) / scaleX_;
        framebufferPos.y = (screenPos.y - offset_.y) / scaleY_;
        return framebufferPos;
    }
} // namespace Elysium

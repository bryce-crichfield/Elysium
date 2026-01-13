#include "Application.h"
#include "Services/Services.h"
#include "Path.h"
#include "imgui.h"
#include "rlImGui.h"
#include "tinyxml2.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>

#include "Common.h"

using namespace tinyxml2;

namespace Elysium
{

Application &Application::GetInstance()
{
    static Application instance;
    return instance;
}

static Application *g_appInstance = nullptr;

void CustomTraceLogCallback(int logLevel, const char *text, va_list args)
{
    if (g_appInstance)
    {
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), text, args);
        g_appInstance->GetService<Elysium::Services::LogService>().LogMessage(logLevel, std::string(buffer));
    }
}

bool Application::Initialize(const std::string &configPath)
{
    Profile;
    if (initialized_)
    {
        return true;
    }

    RegisterService(std::make_unique<Elysium::Services::LogService>());
    RegisterService(std::make_unique<Elysium::Services::AssetService>());
    RegisterService(std::make_unique<Elysium::Services::WorldService>());
    RegisterService(std::make_unique<Elysium::Services::LoadingService>());
    RegisterService(std::make_unique<Elysium::Services::SceneService>());
    RegisterService(std::make_unique<Elysium::Services::TimelineService>());

    g_appInstance = this;
    SetTraceLogCallback(CustomTraceLogCallback);
    SetTraceLogLevel(LOG_DEBUG);

    if (!ApplicationConfig::FromXML(configPath, config_))
    {
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

    for (auto service : serviceRegistry_.GetAllServices()) {
        service->Initialize();
    }

    initialized_ = true;
    LOG_INFO("Application", "Engine initialization complete");
    return true;
}

void Application::Run()
{
    Profile;
    if (!initialized_)
    {
        TraceLog(LOG_ERROR, "Application not initialized!");
        return;
    }

    while (!WindowShouldClose() && !shouldClose_)
    {
        #ifdef TRACY_ENABLE
            FrameMark;
        #endif
        
        ProfileN("Frame");

        float deltaTime = GetFrameTime();

        if (IsWindowResized())
        {
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
    Profile;
    if (!initialized_)
    {
        return;
    }

    g_appInstance = nullptr;

    for (auto service : serviceRegistry_.GetAllServices()) {
        service->Shutdown();
    }

    UnloadRenderTexture(frontBuffer_);
    UnloadRenderTexture(backBuffer_);
    UnloadRenderTexture(sceneFramebuffer_);
    UnloadRenderTexture(transitionBuffer_);

    rlImGuiShutdown();
    CloseAudioDevice();
    CloseWindow();

    initialized_ = false;
}

bool Application::ShouldClose() const
{
    return shouldClose_ || WindowShouldClose();
}

void Application::Update(float deltaTime)
{
    Profile;
    for (auto service : serviceRegistry_.GetAllServices()) {
        service->Update(deltaTime);
    }

    auto &loadingService_ = serviceRegistry_.GetService<Elysium::Services::LoadingService>();
    auto &sceneService_ = serviceRegistry_.GetService<Elysium::Services::SceneService>();
    auto &assetService_ = serviceRegistry_.GetService<Elysium::Services::AssetService>();

    // Handle asset loading during transitions
    static bool loadingStarted = false;
    const std::string &currentState = sceneService_.GetCurrentState();

    if (currentState == "LOADING_ASSETS" && !loadingStarted)
    {
        // Start loading assets when we enter LOADING_ASSETS state
        const auto &pendingAssets = sceneService_.GetPendingAssets();
        LOG_INFOF("Application", "Application received %zu pending assets from SceneService", pendingAssets.size());
        if (!pendingAssets.empty())
        {
            LOG_INFO("Application", "Starting asset loading via LoadingService");
            loadingService_.LoadAssets(pendingAssets);
        }
        else
        {
            LOG_WARNING("Application", "No pending assets to load - scene may not have defined any assets");
        }
        loadingStarted = true;
    }
    else if (currentState == "LOADING_ASSETS")
    {
        // Check if we're done loading
        if (loadingService_.IsComplete())
        {
            // Loading complete, finalize GPU resources on main thread
            assetService_.FinalizeAssets();

            // Signal scene service that loading is complete
            sceneService_.OnAssetsLoaded();
        }
    }
    else if (currentState != "LOADING_ASSETS" && loadingStarted)
    {
        LOG_INFOF("Application", "Resetting loadingStarted flag (state: %s)", currentState.c_str());
        loadingStarted = false;
    }
}

void Application::Draw()
{
    Profile;
    auto &loadingService_ = serviceRegistry_.GetService<Elysium::Services::LoadingService>();
    auto &sceneService_ = serviceRegistry_.GetService<Elysium::Services::SceneService>();
    auto &assetService_ = serviceRegistry_.GetService<Elysium::Services::AssetService>();

    auto renderStart = std::chrono::high_resolution_clock::now();
    auto screenRect = Rectangle{0, 0, config_.framebufferWidth, config_.framebufferHeight};
    Scene *currentScene = sceneService_.GetScene();

    const std::string &drawState = sceneService_.GetCurrentState();

    if (sceneService_.IsTransitioning())
    {
        if (drawState == "LOADING_ASSETS")
        {
            BeginDrawing();
            ClearBackground(BLACK);
            int screenWidth = GetScreenWidth();
            int screenHeight = GetScreenHeight();

            // Delegate drawing to LoadingService
            // loadingService_.Draw(screenWidth, screenHeight);
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

            if (drawState == "EXITING")
            {
                Color currentTint = {255, 255, 255, (unsigned char)(255 * (1.0f - alpha))};
                DrawTexturePro(
                    sceneFramebuffer_.texture,
                    Rectangle{0, 0, (float)sceneFramebuffer_.texture.width, -(float)sceneFramebuffer_.texture.height},
                    letterboxRect_, Vector2{0, 0}, 0.0f, currentTint);
            }
            else if (drawState == "ENTERING")
            {
                Color pendingTint = {255, 255, 255, (unsigned char)(255 * alpha)};
                DrawTexturePro(
                    sceneFramebuffer_.texture,
                    Rectangle{0, 0, (float)sceneFramebuffer_.texture.width, -(float)sceneFramebuffer_.texture.height},
                    letterboxRect_, Vector2{0, 0}, 0.0f, pendingTint);
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
            letterboxRect_, Vector2{0, 0}, 0.0f, WHITE);
    }

    rlImGuiBegin();


    for (auto &service : serviceRegistry_.GetAllServices())
    {
        service->DebugDraw();
    }

    // TODO: Add OnSceneChanged listener so Inspector knows to observe the new scene

    rlImGuiEnd();

    EndDrawing();

    auto renderEnd = std::chrono::high_resolution_clock::now();
    float renderTime = std::chrono::duration<float>(renderEnd - renderStart).count();
}

void Application::ProcessEvents()
{
    Profile;
}

void Application::ProcessInput()
{
    Profile;
    ImGuiIO &io = ImGui::GetIO();

    if (IsKeyPressed(KEY_F1))
    {
        serviceRegistry_.GetService<Elysium::Services::SceneService>().ToggleVisibility();
    }

    if (IsKeyPressed(KEY_F3))
    {
        serviceRegistry_.GetService<Elysium::Services::WorldService>().ToggleVisibility();
    }

    if (IsKeyPressed(KEY_F2))
    {
        serviceRegistry_.GetService<Elysium::Services::LogService>().ToggleVisibility();
    }

    if (IsKeyPressed(KEY_F4))
    {
        serviceRegistry_.GetService<Elysium::Services::TimelineService>().ToggleVisibility();
    }
}

bool ApplicationConfig::FromXML(const std::string &configPath, ApplicationConfig &out)
{
    ApplicationConfig &config = out;
    XMLDocument doc;

    if (doc.LoadFile(Path(configPath).c_str()) != XML_SUCCESS)
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

            TraceLog(LOG_INFO, "Color: %d, %d, %d", config.backgroundColor.r, config.backgroundColor.g,
                     config.backgroundColor.b);
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

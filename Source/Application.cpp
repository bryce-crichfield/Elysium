#include "Application.h"
#include "Services/Services.h"
#include "Path.h"
#include "imgui.h"
#include "rlImGui.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>

#include "Common.h"

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
    RegisterService(std::make_unique<Elysium::Services::EventService>());
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
    static bool wasLoadingPrevFrame = false;  // Track if loading service was busy last frame
    Elysium::Services::SceneState currentState = sceneService_.GetCurrentState();
    bool isLoadingNow = loadingService_.IsProcessing();

    if (currentState == Elysium::Services::SceneState::LOADING_ASSETS && !loadingStarted)
    {
        // We basically submit a request to the loading service, to load some assets
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
    else if (currentState == Elysium::Services::SceneState::LOADING_ASSETS)
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

    // Finalize assets when loading completes outside of scene loading
    // This handles assets loaded manually via AssetService UI
    if (currentState != Elysium::Services::SceneState::LOADING_ASSETS)
    {
        // Detect transition from loading to complete
        if (wasLoadingPrevFrame && !isLoadingNow && loadingService_.IsComplete())
        {
            LOG_INFO("Application", "Manual asset loading complete, finalizing assets");
            assetService_.FinalizeAssets();
        }
    }

    if (currentState != Elysium::Services::SceneState::LOADING_ASSETS && loadingStarted)
    {
        LOG_INFOF("Application", "Resetting loadingStarted flag (state: %s)",
                  Elysium::Services::SceneStateToString(currentState));
        loadingStarted = false;
    }

    // Track loading state for next frame
    wasLoadingPrevFrame = isLoadingNow;
}

void Application::Draw()
{
    Profile;

    // Begin frame
    BeginDrawing();
    ClearBackground(BLACK);

    // Services render their content (SceneService draws scenes, etc.)
    for (auto& service : serviceRegistry_.GetAllServices())
    {
        service->Render();
    }

    // ImGui overlays on top
    rlImGuiBegin();
    for (auto& service : serviceRegistry_.GetAllServices())
    {
        service->DebugDraw();
    }
    rlImGuiEnd();

    EndDrawing();
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

    if (IsKeyPressed(KEY_F5))
    {
        serviceRegistry_.GetService<Elysium::Services::AssetService>().ToggleVisibility();
    }
}

} // namespace Elysium

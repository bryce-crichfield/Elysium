#include "Application.h"
#include "Services/Services.h"
#include "Path.h"
#include "imgui.h"
#include "rlImGui.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <thread>
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
    RegisterService(std::make_unique<Elysium::Services::MessageService>());
    RegisterService(std::make_unique<Elysium::Services::NetworkService>());
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

    // Finalize assets when loading completes (for assets loaded manually via AssetService UI)
    auto& loadingService = serviceRegistry_.GetService<Elysium::Services::LoadingService>();
    auto& assetService = serviceRegistry_.GetService<Elysium::Services::AssetService>();

    static bool wasLoadingPrevFrame = false;
    bool isLoadingNow = loadingService.IsProcessing();

    if (wasLoadingPrevFrame && !isLoadingNow && loadingService.IsComplete())
    {
        LOG_INFO("Application", "Asset loading complete, finalizing assets");
        assetService.FinalizeAssets();
    }

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

    if (IsKeyPressed(KEY_F6))
    {
        serviceRegistry_.GetService<Elysium::Services::NetworkService>().ToggleVisibility();
    }
}

} // namespace Elysium

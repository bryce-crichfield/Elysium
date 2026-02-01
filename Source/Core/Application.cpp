#include "Application.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <thread>
#include "Common.h"
#include "Core/Path.h"
#include "Services/Services.h"
#include "Services/ScriptService.h"
#include "Editor/SceneEditor.h"
#include "Editor/WorldEditor.h"
#include "Editor/LogEditor.h"
#include "Editor/AssetEditor.h"
#include "Editor/NetworkEditor.h"
#include "Editor/ScriptEditor.h"
#include "imgui.h"
#include "rlImGui.h"

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
        g_appInstance->GetService<Elysium::Services::LogService>().LogMessage(logLevel, std::string(buffer));
    }
}

bool Application::Initialize(const std::string& configPath) {
    Profile;
    if (initialized_) {
        return true;
    }

    RegisterService(std::make_unique<Elysium::Services::LogService>());
    RegisterService(std::make_unique<Elysium::Services::MessageService>());
    RegisterService(std::make_unique<Elysium::Services::NetworkService>());
    RegisterService(std::make_unique<Elysium::Services::InvokeService>());
    RegisterService(std::make_unique<Elysium::Services::AssetService>());
    RegisterService(std::make_unique<Elysium::Services::WorldService>());
    RegisterService(std::make_unique<Elysium::Services::LoadingService>());
    RegisterService(std::make_unique<Elysium::Services::SceneService>());
    RegisterService(std::make_unique<Elysium::Services::ScriptService>());

    RegisterEditor<SceneEditor>();
    RegisterEditor<WorldEditor>();
    RegisterEditor<LogEditor>();
    RegisterEditor<AssetEditor>();
    RegisterEditor<NetworkEditor>();
    RegisterEditor<ScriptEditor>();

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

    for (auto service : serviceRegistry_.GetAllServices()) {
        service->Initialize();
    }

    for (auto& editor : editors_) {
        editor->Initialize();
    }

    initialized_ = true;
    LOG_INFO("Application", "Engine initialization complete");
    return true;
}

void Application::Run() {
    Profile;
    if (!initialized_) {
        TraceLog(LOG_ERROR, "Application not initialized!");
        return;
    }

    while (!WindowShouldClose() && !shouldClose_) {
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

void Application::Shutdown() {
    Profile;
    if (!initialized_) {
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

bool Application::ShouldClose() const {
    return shouldClose_ || WindowShouldClose();
}

void Application::Update(float deltaTime) {
    Profile;
    for (auto service : serviceRegistry_.GetAllServices()) {
        service->Update(deltaTime);
    }

    // Finalize assets when loading completes (for assets loaded manually via AssetService UI)
    auto& loadingService = serviceRegistry_.GetService<Elysium::Services::LoadingService>();
    auto& assetService = serviceRegistry_.GetService<Elysium::Services::AssetService>();

    static bool wasLoadingPrevFrame = false;
    bool isLoadingNow = loadingService.IsProcessing();

    if (wasLoadingPrevFrame && !isLoadingNow && loadingService.IsComplete()) {
        LOG_INFO("Application", "Asset loading complete, finalizing assets");
        assetService.FinalizeAssets();
    }

    wasLoadingPrevFrame = isLoadingNow;
}

void Application::Draw() {
    Profile;

    if (pendingFontReload_) {
        ImGui::GetIO().Fonts->Clear();
        rlImGuiBeginInitImGui();
        rlImGuiEndInitImGui();
        for (auto& editor : editors_) {
            editor->Initialize();
        }
        pendingFontReload_ = false;
    }

    // Begin frame
    BeginDrawing();
    ClearBackground(BLACK);

    // Services render their content (SceneService draws scenes, etc.)
    for (auto& service : serviceRegistry_.GetAllServices()) {
        service->Render();
    }

    // ImGui overlays on top
    rlImGuiBegin();
    for (auto& editor : editors_) {
        if (editor->IsVisible()) {
            editor->Draw(*this);
        }
    }
    rlImGuiEnd();

    EndDrawing();
}

void Application::ProcessEvents() {
    Profile;
}

void Application::ProcessInput() {
    Profile;

    if (IsKeyPressed(KEY_F1)) {
        if (auto* editor = GetEditor<SceneEditor>()) {
            editor->ToggleVisibility();
        }
    }

    if (IsKeyPressed(KEY_F2)) {
        if (auto* editor = GetEditor<LogEditor>()) {
            editor->ToggleVisibility();
        }
    }

    if (IsKeyPressed(KEY_F3)) {
        if (auto* editor = GetEditor<WorldEditor>()) {
            editor->ToggleVisibility();
        }
    }

    if (IsKeyPressed(KEY_F5)) {
        if (auto* editor = GetEditor<AssetEditor>()) {
            editor->ToggleVisibility();
        }
    }

    if (IsKeyPressed(KEY_F6)) {
        if (auto* editor = GetEditor<NetworkEditor>()) {
            editor->ToggleVisibility();
        }
    }

    if (IsKeyPressed(KEY_F7)) {
        if (auto* editor = GetEditor<ScriptEditor>()) {
            editor->ToggleVisibility();
        }
    }
}

}  // namespace Elysium

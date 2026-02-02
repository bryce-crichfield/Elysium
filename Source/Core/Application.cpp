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
#include "imgui_internal.h"
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
    startTime_ += deltaTime;

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

    // Services render their content (SceneService draws scenes to framebuffer)
    for (auto& service : serviceRegistry_.GetAllServices()) {
        service->Render();
    }

    // ImGui overlays
    rlImGuiBegin();

    if (mode_ == AppMode::Editor) {
        // Enable docking and prevent tab close via middle-click
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::GetIO().ConfigDockingAlwaysTabBar = true;

        // Full-window dockspace
        ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

        // Build default layout once
        if (!editorLayoutBuilt_) {
            editorLayoutBuilt_ = true;

            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

            // Split: left 20% | remainder
            ImGuiID dockLeft, dockRemain;
            ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.20f, &dockLeft, &dockRemain);

            // Split remainder: bottom 25% | center+right
            ImGuiID dockBottom, dockCenterRight;
            ImGui::DockBuilderSplitNode(dockRemain, ImGuiDir_Down, 0.25f, &dockBottom, &dockCenterRight);

            // Split center+right: center | right 25%
            ImGuiID dockCenter, dockRight;
            ImGui::DockBuilderSplitNode(dockCenterRight, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);

            // Assign windows
            ImGui::DockBuilderDockWindow("World Editor", dockLeft);
            ImGui::DockBuilderDockWindow("Game", dockCenter);
            ImGui::DockBuilderDockWindow("Log Viewer", dockBottom);
            ImGui::DockBuilderDockWindow("Scene Editor", dockRight);
            ImGui::DockBuilderDockWindow("Asset Browser", dockRight);
            ImGui::DockBuilderDockWindow("Network", dockRight);
            ImGui::DockBuilderDockWindow("Script Editor", dockCenter);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        // Game viewport panel
        auto& sceneService = Application::GetInstance().GetService<Services::SceneService>();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::Begin("Game", nullptr, ImGuiWindowFlags_NoCollapse)) {
            // Grab content region position and size before drawing the image
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 avail = ImGui::GetContentRegionAvail();

            rlImGuiImageRenderTextureFit(&sceneService.GetFramebuffer(), true);

            // Tell SceneService where the game viewport is on screen
            // rlImGuiImageRenderTextureFit centers the image maintaining aspect ratio
            float fbW = (float)sceneService.GetFramebuffer().texture.width;
            float fbH = (float)sceneService.GetFramebuffer().texture.height;
            float fbAspect = fbW / fbH;
            float regionAspect = avail.x / avail.y;

            float drawW, drawH;
            if (fbAspect > regionAspect) {
                drawW = avail.x;
                drawH = avail.x / fbAspect;
            } else {
                drawH = avail.y;
                drawW = avail.y * fbAspect;
            }
            float drawX = pos.x + (avail.x - drawW) * 0.5f;
            float drawY = pos.y + (avail.y - drawH) * 0.5f;

            LOG_DEBUGF("SceneService", "Viewport Rect: x=%.1f y=%.1f w=%.1f h=%.1f", drawX, drawY, drawW, drawH);
            sceneService.SetViewportRect(Rectangle{drawX, drawY, drawW, drawH});
        }
        ImGui::End();
        ImGui::PopStyleVar();
    } else {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;

        auto& sceneService = Application::GetInstance().GetService<Services::SceneService>();
        auto& texture = sceneService.GetFramebuffer().texture;
        auto letterboxRect = sceneService.GetLetterboxRect();
        DrawTexturePro(
            texture,
            Rectangle{0, 0, (float)texture.width, -(float)texture.height},
            letterboxRect, Vector2{0, 0}, 0.0f, WHITE);
        sceneService.SetViewportRect(letterboxRect);
    }

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

void Application::SetMode(AppMode mode) {
    if (mode_ == mode) return;
    mode_ = mode;

    if (mode_ == AppMode::Editor) {
        for (auto& editor : editors_) {
            editor->SetVisible(true);
        }
    } else {
        for (auto& editor : editors_) {
            editor->SetVisible(false);
        }
        editorLayoutBuilt_ = false;
    }
}

void Application::ProcessInput() {
    Profile;

    if (IsKeyPressed(KEY_F1)) {
        SetMode(AppMode::Editor);
    }

    if (IsKeyPressed(KEY_F2)) {
        SetMode(AppMode::Play);
    }
}

}  // namespace Elysium

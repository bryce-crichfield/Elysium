#include "Application.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <thread>
#include "Common.h"
#include "Core/Path.h"
#include "Interfaces/ILogService.h"
#include "Interfaces/ISceneService.h"
#include "Services/ApplicationService.h"
#include "Services/Services.h"
#include "Services/ScriptService.h"
#include "Editor/SceneEditor.h"
#include "Editor/WorldEditor.h"
#include "Editor/LogEditor.h"
#include "Editor/AssetEditor.h"
#include "Editor/NetworkEditor.h"
#include "Editor/ScriptEditor.h"
#include "Editor/ViewportEditor.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"

namespace Elysium {

static Application* g_appInstance = nullptr;

void CustomTraceLogCallback(int logLevel, const char* text, va_list args) {
    if (g_appInstance) {
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), text, args);
        g_appInstance->GetServiceLocator().Get<Services::ILogService>().LogMessage(logLevel, std::string(buffer));
    }
}

bool Application::Initialize(const std::string& configPath) {
    Profile;
    if (initialized_) {
        return true;
    }

    serviceLocator_.Register<Services::ApplicationService, Services::IApplicationService>(
        std::make_unique<Services::ApplicationService>(serviceLocator_, *this));
    serviceLocator_.Register<Services::LogService, Services::ILogService>(
        std::make_unique<Services::LogService>(serviceLocator_));
    serviceLocator_.Register<Services::MessageService, Services::IMessageService>(
        std::make_unique<Services::MessageService>(serviceLocator_));
    serviceLocator_.Register<Services::NetworkService, Services::INetworkService>(
        std::make_unique<Services::NetworkService>(serviceLocator_));
    serviceLocator_.Register<Services::InvokeService, Services::IInvokeService>(
        std::make_unique<Services::InvokeService>(serviceLocator_));
    serviceLocator_.Register<TaskService, Services::ITaskService>(
        std::make_unique<TaskService>(serviceLocator_));
    serviceLocator_.Register<Services::AssetService, Services::IAssetService>(
        std::make_unique<Services::AssetService>(serviceLocator_));
    serviceLocator_.Register<Services::EditorService, Services::IEditorService>(
        std::make_unique<Services::EditorService>(serviceLocator_));
    serviceLocator_.Register<Services::SceneService, Services::ISceneService>(
        std::make_unique<Services::SceneService>(serviceLocator_));
    serviceLocator_.Register<Services::ScriptService, Services::IScriptService>(
        std::make_unique<Services::ScriptService>(serviceLocator_));

    RegisterEditor<SceneEditor>();
    RegisterEditor<WorldEditor>();
    RegisterEditor<LogEditor>();
    RegisterEditor<AssetEditor>();
    RegisterEditor<NetworkEditor>();
    RegisterEditor<ScriptEditor>();
    RegisterEditor<ViewportEditor>();

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
    MaximizeWindow();
    SetExitKey(0);  // Disable raylib's default ESC-to-quit; handled by scene scripts

    InitAudioDevice();

    rlImGuiSetup(true);
    // SetTargetFPS(config_.targetFPS);

    // Must be set before the first ImGui::NewFrame() (rlImGuiBegin() below), or ImGui
    // asserts — docking state only matters once editor windows exist (Editor mode),
    // but the flag itself is harmless to leave enabled in Play mode.
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().ConfigDockingAlwaysTabBar = true;


    for (auto service : serviceLocator_.GetAllServices()) {
        service->Initialize();
    }

    for (auto& editor : editors_) {
        editor->Initialize(config_);
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

    for (auto service : serviceLocator_.GetAllServices()) {
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

    for (auto service : serviceLocator_.GetAllServices()) {
        service->Update(deltaTime);
    }

}

void Application::DrawMenuBar()
{
    // Add menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                // TODO: Handle new scene
            }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
                // TODO: Handle open scene
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                // TODO: Handle save scene
            }
            if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S")) {
                
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                shouldClose_ = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                // Handle undo
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                // Handle redo
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            for (auto& editor : editors_) {
                bool visible = editor->IsVisible();
                if (ImGui::MenuItem(editor->GetName().c_str(), nullptr, &visible)) {
                    editor->SetVisible(visible);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Mode")) {
            if (ImGui::MenuItem("Editor", "F1", mode_ == AppMode::Editor)) {
                SetMode(AppMode::Editor);
            }
            if (ImGui::MenuItem("Play", "F2", mode_ == AppMode::Play)) {
                SetMode(AppMode::Play);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void Application::Draw() {
    Profile;

    if (pendingFontReload_) {
        ImGui::GetIO().Fonts->Clear();
        rlImGuiBeginInitImGui();
        rlImGuiEndInitImGui();
        for (auto& editor : editors_) {
            editor->Initialize(config_);
        }
        pendingFontReload_ = false;
    }   

    // Begin frame
    BeginDrawing();
    ClearBackground(BLACK);

    // Services render their content (SceneService draws scenes to framebuffer)
    for (auto& service : serviceLocator_.GetAllServices()) {
        service->Render();
    }

    // ImGui overlays
    rlImGuiBegin();

    if (mode_ == AppMode::Editor) {
        DrawMenuBar();
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
            ImGui::DockBuilderDockWindow("Script Editor", dockCenter);
            ImGui::DockBuilderDockWindow("Game", dockCenter);
            ImGui::DockBuilderDockWindow("Log Viewer", dockBottom);
            ImGui::DockBuilderDockWindow("Scene Editor", dockRight);
            ImGui::DockBuilderDockWindow("Asset Browser", dockRight);
            ImGui::DockBuilderDockWindow("Network", dockRight);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        // Game viewport panel is drawn by ViewportEditor, part of the generic editors loop below.
    } else {
        auto& sceneService = serviceLocator_.Get<Services::ISceneService>();
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
            editor->Draw();
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

    auto& sceneService = serviceLocator_.Get<Services::ISceneService>();

    if (mode_ == AppMode::Editor) {
        for (auto& editor : editors_) {
            editor->SetVisible(true);
        }

        // Editing should start paused by default — the user opts into simulating via the Play button.
        sceneService.SetPlaying(false);
    } else {
        for (auto& editor : editors_) {
            editor->SetVisible(false);
        }
        editorLayoutBuilt_ = false;

        // Fullscreen Play mode always simulates.
        sceneService.SetPlaying(true);
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

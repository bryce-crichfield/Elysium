#include "Application.h"
#include <chrono>
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
#include "Core/Input.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium {

bool ApplicationConfig::FromXML(const std::string& configPath, ApplicationConfig& out) {
    ApplicationConfig& config = out;
    XMLDocument doc;

    if (doc.LoadFile(Path(configPath).c_str()) != XML_SUCCESS) {
        LOG_ERRORF("Application", "Failed to load config file: %s. Using defaults.", configPath.c_str());
        return false;
    }

    XMLElement* root = doc.FirstChildElement("GameConfig");
    if (!root) {
        LOG_ERROR("Application", "Invalid config file format. Using defaults.");
        return false;
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
        if (XMLElement* backgroundColor = window->FirstChildElement("BackgroundColor")) {
            config.backgroundColor = Colors::Black;
            if (XMLElement* r = backgroundColor->FirstChildElement("r"))
                config.backgroundColor.r = r->IntText(255);
            if (XMLElement* g = backgroundColor->FirstChildElement("g"))
                config.backgroundColor.g = g->IntText(255);
            if (XMLElement* b = backgroundColor->FirstChildElement("b"))
                config.backgroundColor.b = b->IntText(255);

            LOG_INFOF("Application", "Color: %d, %d, %d", config.backgroundColor.r, config.backgroundColor.g,
                      config.backgroundColor.b);
        }
        if (XMLElement* framebuffer = window->FirstChildElement("Framebuffer")) {
            if (XMLElement* width = framebuffer->FirstChildElement("Width"))
                config.framebufferWidth = width->IntText(640);
            if (XMLElement* height = framebuffer->FirstChildElement("Height"))
                config.framebufferHeight = height->IntText(480);
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

    if (XMLElement* editor = root->FirstChildElement("Editor")) {
        if (XMLElement* fontName = editor->FirstChildElement("FontName"))
            config.editorFontName = fontName->GetText() ? fontName->GetText() : "Hermit-Regular.otf";
    }

    LOG_INFOF("Application", "Loaded game config from: %s", configPath.c_str());

    return true;
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

    if (!ApplicationConfig::FromXML(configPath, config_)) {
        LOG_ERROR("Application", "Failed to load ApplicationConfig.xml");
        return false;
    }

    LOG_INFO("Application", "Elysium Engine initializing");

    window_ = Window(config_.windowWidth, config_.windowHeight, config_.windowTitle);
    window_.Maximize();

    // Audio device init deferred until the real audio backend (miniaudio) lands.

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
        LOG_ERROR("Application", "Application not initialized!");
        return;
    }

    while (!window_.ShouldClose() && !shouldClose_) {
#ifdef TRACY_ENABLE
        FrameMark;
#endif

        ProfileN("Frame");

        float deltaTime = window_.GetDeltaTime();

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

    for (auto service : serviceLocator_.GetAllServices()) {
        service->Shutdown();
    }

    rlImGuiShutdown();
    window_ = Window();

    initialized_ = false;
}

bool Application::ShouldClose() const {
    return shouldClose_ || window_.ShouldClose();
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
    window_.BeginFrame(Colors::Black);

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
        sceneService.Present(sceneService.GetLetterboxRect());
    }

    for (auto& editor : editors_) {
        if (editor->IsVisible()) {
            editor->Draw();
        }
    }

    rlImGuiEnd();

    window_.EndFrame();
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

    if (Input::IsKeyPressed(Key::F1)) {
        SetMode(AppMode::Editor);
    }

    if (Input::IsKeyPressed(Key::F2)) {
        SetMode(AppMode::Play);
    }
}

}  // namespace Elysium

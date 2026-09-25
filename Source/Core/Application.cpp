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
#include "Editor/ScenesEditor.h"
#include "Editor/HierarchyEditor.h"
#include "Editor/InspectorEditor.h"
#include "Editor/LogEditor.h"
#include "Editor/AssetEditor.h"
#include "Editor/NetworkEditor.h"
#include "Editor/ScriptEditor.h"
#include "Editor/ViewportEditor.h"
#include "Editor/Theme.h"
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

    // Registration order is tab order within each dock node.
    RegisterEditor<ScenesEditor>();
    RegisterEditor<HierarchyEditor>();
    RegisterEditor<InspectorEditor>();
    RegisterEditor<SceneEditor>();
    RegisterEditor<ViewportEditor>();
    RegisterEditor<ScriptEditor>();
    RegisterEditor<LogEditor>();
    RegisterEditor<AssetEditor>();
    RegisterEditor<NetworkEditor>();

    if (!ApplicationConfig::FromXML(configPath, config_)) {
        LOG_ERROR("Application", "Failed to load ApplicationConfig.xml");
        return false;
    }

    LOG_INFO("Application", "Elysium Engine initializing");

    window_ = Window(config_.windowWidth, config_.windowHeight, config_.windowTitle);
    window_.Maximize();

    // Audio device init deferred until the real audio backend (miniaudio) lands.

    rlImGuiSetup(true);
    EditorStyle::LoadTheme("Dark");
    EditorStyle::LoadFonts();
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
            // Docked panels are fixed; only the dialogs are opened from here.
            for (auto& editor : editors_) {
                if (editor->IsDocked()) continue;
                if (ImGui::MenuItem(editor->GetName().c_str())) editor->SetVisible(true);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                // Re-read on selection, so edits to the file show up without a restart.
                for (const std::string& name : EditorStyle::AvailableThemes()) {
                    if (ImGui::MenuItem(name.c_str(), nullptr, name == EditorStyle::CurrentTheme().name) &&
                        EditorStyle::LoadTheme(name)) {
                        RequestFontReload();
                    }
                }
                ImGui::EndMenu();
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
        EditorStyle::LoadFonts();
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
        // Rebuilt whenever the viewport resizes (e.g. the window maximizing after the first
        // frame): otherwise the central node soaks up all the growth and the side columns
        // stay at their first-frame width.
        const ImVec2 workSize = ImGui::GetMainViewport()->WorkSize;
        if (!editorLayoutBuilt_ || workSize.x != editorLayoutWidth_ || workSize.y != editorLayoutHeight_) {
            editorLayoutBuilt_ = true;
            editorLayoutWidth_ = workSize.x;
            editorLayoutHeight_ = workSize.y;
            focusDefaultTabs_ = true;

            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

            // Scenes/Hierarchy left, Inspector/Scene right, Console/Assets along the bottom
            // of the middle, Viewport/Scripts in the center. The layout is fixed: panels
            // can't be closed, moved or undocked (see Editor::BeginWindow).
            ImGuiID dockLeft, dockRemain;
            ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.18f, &dockLeft, &dockRemain);
            ImGuiID dockRight, dockMiddle;
            ImGui::DockBuilderSplitNode(dockRemain, ImGuiDir_Right, 0.26f, &dockRight, &dockMiddle);
            ImGuiID dockBottom, dockCenter;
            ImGui::DockBuilderSplitNode(dockMiddle, ImGuiDir_Down, 0.28f, &dockBottom, &dockCenter);

            ImGui::DockBuilderDockWindow(ScenesEditor::Title, dockLeft);
            ImGui::DockBuilderDockWindow(HierarchyEditor::Title, dockLeft);
            ImGui::DockBuilderDockWindow(ViewportEditor::Title, dockCenter);
            ImGui::DockBuilderDockWindow(ScriptEditor::Title, dockCenter);
            ImGui::DockBuilderDockWindow(LogEditor::Title, dockBottom);
            ImGui::DockBuilderDockWindow(AssetEditor::Title, dockBottom);
            ImGui::DockBuilderDockWindow(InspectorEditor::Title, dockRight);
            ImGui::DockBuilderDockWindow(SceneEditor::Title, dockRight);

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

    // Tabs open on Hierarchy, Inspector, Viewport and Console; the panels have to exist first.
    if (focusDefaultTabs_ && mode_ == AppMode::Editor) {
        focusDefaultTabs_ = false;
        for (const char* title : {HierarchyEditor::Title, InspectorEditor::Title, LogEditor::Title, ViewportEditor::Title}) {
            ImGui::SetWindowFocus(title);
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
            editor->SetVisible(editor->IsDocked());
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

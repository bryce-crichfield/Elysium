#include "Application.h"
#include <chrono>
#include <thread>
#include "Common.h"
#include "Core/Path.h"
#include "Interfaces/IAudioService.h"
#include "Interfaces/ILogService.h"
#include "Interfaces/ISceneService.h"
#include "Services/ApplicationService.h"
#include "Services/Services.h"
#include "Core/Input.h"
#include "Editor/EditorApplication.h"
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

    LOG_INFOF("Application", "Loaded game config from: %s", configPath.c_str());

    return true;
}

Application::Application() = default;
Application::~Application() = default;

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
    serviceLocator_.Register<Services::AudioService, Services::IAudioService>(
        std::make_unique<Services::AudioService>(serviceLocator_));
    serviceLocator_.Register<Services::SceneService, Services::ISceneService>(
        std::make_unique<Services::SceneService>(serviceLocator_));
    serviceLocator_.Register<Services::ScriptService, Services::IScriptService>(
        std::make_unique<Services::ScriptService>(serviceLocator_));

    if (!ApplicationConfig::FromXML(configPath, config_)) {
        LOG_ERROR("Application", "Failed to load ApplicationConfig.xml");
        return false;
    }

    LOG_INFO("Application", "Elysium Engine initializing");

    window_ = Window(config_.windowWidth, config_.windowHeight, config_.windowTitle);
    window_.Maximize();

    for (auto service : serviceLocator_.GetAllServices()) {
        service->Initialize();
    }

    editor_ = std::make_unique<EditorApplication>(serviceLocator_);
    editor_->Initialize(config_);

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

    if (editor_) editor_->Shutdown();
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
    if (editor_) editor_->Update(deltaTime);

}

void Application::Draw() {
    Profile;

    window_.BeginFrame(Colors::Black);

    // Services render their content (SceneService draws scenes to framebuffer)
    for (auto& service : serviceLocator_.GetAllServices()) {
        service->Render();
    }

    // In Play the game fills the window; in Editor the Viewport panel shows it.
    if (mode_ == AppMode::Play) {
        auto& sceneService = serviceLocator_.Get<Services::ISceneService>();
        sceneService.Present(sceneService.GetLetterboxRect());
    }

    if (editor_) editor_->Draw(mode_);

    window_.EndFrame();
}

void Application::ProcessEvents() {
    Profile;
}

void Application::SetMode(AppMode mode) {
    if (mode_ == mode) return;
    mode_ = mode;

    if (editor_) editor_->OnModeChanged(mode_);

    // Neither side's sounds carry over: the game's music shouldn't play under the editor, nor an
    // editor preview into the game.
    serviceLocator_.Get<Services::IAudioService>().StopAll();

    // The editor works on its own document copies; the game's stack just freezes. Play runs
    // what's on disk, never the editor's in-memory copies.
    if (mode_ == AppMode::Play) {
        serviceLocator_.Get<Services::ISceneService>().ReloadFromDisk();
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

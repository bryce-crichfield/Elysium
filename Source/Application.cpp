#include "Application.h"
#include "rlImGui.h"
#include "imgui.h"
#include "tinyxml2.h"
#include <cstdio>
#include <cstdarg>
#include <chrono>

using namespace tinyxml2;

namespace Elysium
{

    Application &Application::GetInstance()
    {
        static Application instance;
        return instance;
    }

    void CustomTraceLogCallback(int logLevel, const char *text, va_list args)
    {
        Services::LogService::GetInstance().LogMessage(logLevel, text, args);
    }

    bool Application::Initialize(const std::string &configPath)
    {
        if (initialized_)
        {
            return true;
        }

        SetTraceLogCallback(CustomTraceLogCallback);
        SetTraceLogLevel(LOG_INFO);

        config_ = LoadGameConfig(configPath);

        TraceLog(LOG_INFO, "Elysium Engine initializing...");

        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(config_.windowWidth, config_.windowHeight, config_.windowTitle.c_str());

        rlImGuiSetup(true);
        SetTargetFPS(config_.targetFPS);

        frontBuffer_ = LoadRenderTexture(config_.windowWidth, config_.windowHeight);
        backBuffer_ = LoadRenderTexture(config_.windowWidth, config_.windowHeight);
        sceneFramebuffer_ = LoadRenderTexture(config_.framebufferWidth, config_.framebufferHeight);

        CalculateLetterboxing();

        assetService_.Initialize();
        networkService_.Initialize();
        Services::LogService::GetInstance().Initialize();

        initialized_ = true;
        return true;
    }

    void Application::Run()
    {
        if (!initialized_)
        {
            TraceLog(LOG_ERROR, "Application not initialized!");
            return;
        }

        while (!WindowShouldClose() && !shouldClose_)
        {
            float deltaTime = GetFrameTime();

            if (IsWindowResized())
            {
                CalculateLetterboxing();
            }

            ProcessInput();
            ProcessEvents();
            HandleSceneTransition();
            Update(deltaTime);
            Draw();
        }
    }

    void Application::Shutdown()
    {
        if (!initialized_)
        {
            return;
        }

        if (currentScene_)
        {
            currentScene_->OnExit();
        }

        UnloadRenderTexture(frontBuffer_);
        UnloadRenderTexture(backBuffer_);
        UnloadRenderTexture(sceneFramebuffer_);

        assetService_.Shutdown();
        networkService_.Shutdown();
        Services::LogService::GetInstance().Shutdown();

        rlImGuiShutdown();
        CloseWindow();

        initialized_ = false;
    }

    void Application::SetScene(std::unique_ptr<Scene> scene)
    {
        if (currentScene_)
        {
            currentScene_->OnExit();
        }
        currentScene_ = std::move(scene);
        if (currentScene_)
        {
            currentScene_->OnEnter();
        }
    }

    void Application::QueueSceneTransition(std::unique_ptr<Scene> scene)
    {
        if (!sceneTransitionLocked_)
        {
            pendingScene_ = std::move(scene);
            sceneTransitionPending_ = true;
        }
    }

    bool Application::ShouldClose() const
    {
        return shouldClose_ || WindowShouldClose();
    }

    void Application::Update(float deltaTime)
    {
        metricsService_.RecordFrameTime(deltaTime);
        metricsService_.Update(deltaTime);
        Services::LogService::GetInstance().Update(deltaTime);

        if (currentScene_)
        {
            currentScene_->OnUpdate(deltaTime);
        }

        networkService_.Update();
    }

    void Application::Draw()
    {
        auto renderStart = std::chrono::high_resolution_clock::now();

        BeginTextureMode(sceneFramebuffer_);
        ClearBackground(config_.backgroundColor);
        if (currentScene_)
        {
            currentScene_->OnDraw();
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

        rlImGuiBegin();

        if (currentScene_)
        {
            currentScene_->OnDebugDraw();
        }

        metricsService_.Draw();
        Services::LogService::GetInstance().Draw();

        rlImGuiEnd();

        EndDrawing();

        auto renderEnd = std::chrono::high_resolution_clock::now();
        float renderTime = std::chrono::duration<float>(renderEnd - renderStart).count();
        metricsService_.RecordRenderTime(renderTime);
    }

    void Application::ProcessEvents()
    {
        while (eventService_.HasInputEvents())
        {
            InputEvent event = eventService_.GetNextInputEvent();
            if (currentScene_)
            {
                currentScene_->OnInput(event);
            }
        }

        while (eventService_.HasNetworkEvents())
        {
            NetworkEvent event = eventService_.GetNextNetworkEvent();
            if (currentScene_)
            {
                currentScene_->OnNetwork(event);
            }
        }
    }

    void Application::HandleSceneTransition()
    {
        if (sceneTransitionPending_ && !sceneTransitionLocked_)
        {
            sceneTransitionLocked_ = true;
            SetScene(std::move(pendingScene_));
            sceneTransitionPending_ = false;
            sceneTransitionLocked_ = false;
        }
    }

    void Application::ProcessInput()
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            InputEvent event;
            event.type = InputEvent::KEY_PRESS;
            event.key = KEY_ESCAPE;
            eventService_.QueueInputEvent(event);
        }

        if (IsKeyPressed(KEY_F2))
        {
            metricsService_.ToggleVisibility();
        }

        if (IsKeyPressed(KEY_F3))
        {
            Services::LogService::GetInstance().ToggleVisibility();
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            InputEvent event;
            event.type = InputEvent::MOUSE_PRESS;
            event.key = MOUSE_LEFT_BUTTON;
            Vector2 mousePos = GetMousePosition();
            Vector2 framebufferPos = MapScreenToFramebuffer(mousePos);
            event.x = framebufferPos.x;
            event.y = framebufferPos.y;
            eventService_.QueueInputEvent(event);
        }
    }

    GameConfig Application::LoadGameConfig(const std::string &configPath)
    {
        GameConfig config;
        XMLDocument doc;

        if (doc.LoadFile(configPath.c_str()) != XML_SUCCESS)
        {
            TraceLog(LOG_ERROR, "Failed to load config file: %s. Using defaults.", configPath.c_str());
            return config;
        }

        XMLElement *root = doc.FirstChildElement("GameConfig");
        if (!root)
        {
            TraceLog(LOG_ERROR, "Invalid config file format. Using defaults.");
            return config;
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

        if (XMLElement *physics = root->FirstChildElement("Physics"))
        {
            if (XMLElement *gravity = physics->FirstChildElement("Gravity"))
                config.gravity = gravity->FloatText(9.81f);
            if (XMLElement *ballRadius = physics->FirstChildElement("DefaultBallRadius"))
                config.defaultBallRadius = ballRadius->FloatText(20.0f);
            if (XMLElement *ballSpeed = physics->FirstChildElement("DefaultBallSpeed"))
            {
                float x = ballSpeed->FirstChildElement("X") ? ballSpeed->FirstChildElement("X")->FloatText(5.0f) : 5.0f;
                float y = ballSpeed->FirstChildElement("Y") ? ballSpeed->FirstChildElement("Y")->FloatText(4.0f) : 4.0f;
                config.defaultBallSpeed = (Vector2){x, y};
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
        return config;
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
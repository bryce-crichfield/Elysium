#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "tinyxml2.h"
#include <string>
#include <cstdio>
#include <cstdarg>

using namespace tinyxml2;

struct GameConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle = "Elysium - 2D Game Engine";
    bool fullscreen = false;
    bool vsync = true;
    int targetFPS = 60;
    
    Color backgroundColor = DARKBLUE;
    bool showFPS = true;
    
    float gravity = 9.81f;
    float defaultBallRadius = 20.0f;
    Vector2 defaultBallSpeed = { 5.0f, 4.0f };
    
    bool showDemoWindow = true;
    bool showMetrics = false;
    std::string logLevel = "INFO";
};

GameConfig LoadGameConfig(const std::string& configPath) {
    GameConfig config;
    XMLDocument doc;
    
    if (doc.LoadFile(configPath.c_str()) != XML_SUCCESS) {
        TraceLog(LOG_ERROR, "Failed to load config file: %s. Using defaults.", configPath.c_str());
        return config;
    }
    
    XMLElement* root = doc.FirstChildElement("GameConfig");
    if (!root) {
        TraceLog(LOG_ERROR, "Invalid config file format. Using defaults.");
        return config;
    }
    
    // Window settings
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
    }
    
    // Graphics settings
    if (XMLElement* graphics = root->FirstChildElement("Graphics")) {
        if (XMLElement* bgColor = graphics->FirstChildElement("BackgroundColor")) {
            int r = bgColor->FirstChildElement("R") ? bgColor->FirstChildElement("R")->IntText(47) : 47;
            int g = bgColor->FirstChildElement("G") ? bgColor->FirstChildElement("G")->IntText(79) : 79;
            int b = bgColor->FirstChildElement("B") ? bgColor->FirstChildElement("B")->IntText(79) : 79;
            int a = bgColor->FirstChildElement("A") ? bgColor->FirstChildElement("A")->IntText(255) : 255;
            config.backgroundColor = (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
        }
        if (XMLElement* showFPS = graphics->FirstChildElement("ShowFPS"))
            config.showFPS = showFPS->BoolText(true);
    }
    
    // Physics settings
    if (XMLElement* physics = root->FirstChildElement("Physics")) {
        if (XMLElement* gravity = physics->FirstChildElement("Gravity"))
            config.gravity = gravity->FloatText(9.81f);
        if (XMLElement* ballRadius = physics->FirstChildElement("DefaultBallRadius"))
            config.defaultBallRadius = ballRadius->FloatText(20.0f);
        if (XMLElement* ballSpeed = physics->FirstChildElement("DefaultBallSpeed")) {
            float x = ballSpeed->FirstChildElement("X") ? ballSpeed->FirstChildElement("X")->FloatText(5.0f) : 5.0f;
            float y = ballSpeed->FirstChildElement("Y") ? ballSpeed->FirstChildElement("Y")->FloatText(4.0f) : 4.0f;
            config.defaultBallSpeed = (Vector2){ x, y };
        }
    }
    
    // Debug settings
    if (XMLElement* debug = root->FirstChildElement("Debug")) {
        if (XMLElement* showDemo = debug->FirstChildElement("ShowDemoWindow"))
            config.showDemoWindow = showDemo->BoolText(true);
        if (XMLElement* showMetrics = debug->FirstChildElement("ShowMetrics"))
            config.showMetrics = showMetrics->BoolText(false);
        if (XMLElement* logLevel = debug->FirstChildElement("LogLevel"))
            config.logLevel = logLevel->GetText() ? logLevel->GetText() : "INFO";
    }
    
    TraceLog(LOG_INFO, "Loaded game config from: %s", configPath.c_str());
    return config;
}

// Custom trace log callback with colors
void CustomTraceLogCallback(int logLevel, const char *text, va_list args)
{
    const char* levelColors[] = {
        "\033[37m", // LOG_ALL - White
        "\033[37m", // LOG_TRACE - White  
        "\033[37m", // LOG_DEBUG - White
        "\033[37m", // LOG_INFO - White
        "\033[93m", // LOG_WARNING - Yellow
        "\033[91m", // LOG_ERROR - Red
        "\033[95m", // LOG_FATAL - Magenta
        "\033[90m"  // LOG_NONE - Dark Gray
    };
    
    const char* levelNames[] = {
        "ALL", "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL", "NONE"
    };
    
    const char* resetColor = "\033[0m";
    
    // Clamp log level to valid range
    int safeLevel = (logLevel >= 0 && logLevel < 8) ? logLevel : 3;
    
    printf("%s[%s] ", levelColors[safeLevel], levelNames[safeLevel]);
    vprintf(text, args);
    printf("%s\n", resetColor);
}

int main()
{
    // Set up colored logging
    SetTraceLogCallback(CustomTraceLogCallback);
    SetTraceLogLevel(LOG_INFO);
    
    // Load configuration from XML
    GameConfig config = LoadGameConfig("./Assets/GameConfig.xml");
    
    // Demo different log levels
    TraceLog(LOG_INFO, "Elysium Engine initializing...");
    TraceLog(LOG_WARNING, "This is a warning message");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(config.windowWidth, config.windowHeight, config.windowTitle.c_str());

    rlImGuiSetup(true);

    SetTargetFPS(config.targetFPS);

    Vector2 ballPosition = { config.windowWidth / 2.0f, config.windowHeight / 2.0f };
    Vector2 ballSpeed = config.defaultBallSpeed;
    float ballRadius = config.defaultBallRadius;

    bool showDemoWindow = config.showDemoWindow;
    bool showMetrics = config.showMetrics;
    Color backgroundColor = config.backgroundColor;

    while (!WindowShouldClose())
    {
        // Update ball physics
        ballPosition.x += ballSpeed.x;
        ballPosition.y += ballSpeed.y;

        // Collision with walls
        if ((ballPosition.x >= (GetScreenWidth() - ballRadius)) || (ballPosition.x <= ballRadius))
            ballSpeed.x *= -1.0f;
        if ((ballPosition.y >= (GetScreenHeight() - ballRadius)) || (ballPosition.y <= ballRadius))
            ballSpeed.y *= -1.0f;

        // Start drawing
        BeginDrawing();

        ClearBackground(backgroundColor);

        // Draw raylib content
        DrawCircleV(ballPosition, ballRadius, MAROON);
        DrawText("Elysium Engine - Raylib + ImGui Demo", 10, 10, 20, RAYWHITE);
        if (config.showFPS) {
            DrawFPS(GetScreenWidth() - 95, 10);
        }

        // Start ImGui frame
        rlImGuiBegin();

        // Demo window
        if (showDemoWindow)
        {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        // Engine control window
        ImGui::Begin("Engine Controls");
        
        ImGui::Text("Welcome to Elysium!");
        ImGui::Text("Config loaded from: Assets/game_config.xml");
        ImGui::Separator();
        
        ImGui::Text("Ball Controls:");
        ImGui::SliderFloat("Ball Radius", &ballRadius, 5.0f, 50.0f);
        ImGui::SliderFloat2("Ball Speed", (float*)&ballSpeed, -10.0f, 10.0f);
        
        ImGui::Separator();
        ImGui::Text("Rendering:");
        ImGui::ColorEdit3("Background Color", (float*)&backgroundColor);
        
        ImGui::Separator();
        ImGui::Text("Debug Windows:");
        ImGui::Checkbox("Show Demo Window", &showDemoWindow);
        ImGui::Checkbox("Show Metrics", &showMetrics);
        
        if (ImGui::Button("Reset Ball Position"))
        {
            ballPosition = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
        }
        
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 
                   1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        
        ImGui::End();

        // Metrics window
        if (showMetrics)
        {
            ImGui::ShowMetricsWindow(&showMetrics);
        }

        // End ImGui frame
        rlImGuiEnd();

        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
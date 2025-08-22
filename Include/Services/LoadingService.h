#pragma once

#include "raylib.h"
#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include "Asset.h"

struct LoadingConfig {
    int delayTime = 150;
    // Progress Bar
    struct {
        int width = 400;
        int height = 25;
        int x = -1; // -1 means center
        int y = 300;
        Color backgroundColor = {64, 64, 64, 255};
        Color borderColor = {128, 128, 128, 255};
        int borderThickness = 2;
        Color fillColor = {0, 255, 128, 255};
    } progressBar;

    // Text
    struct {
        std::string loadingText = "Loading...";
        int fontSize = 48;
        Color color = {255, 255, 255, 255};
        int x = -1; // -1 means center
        int y = 200;

        std::string statusText = "Assets loaded: {loaded} / {total}";
        int statusFontSize = 24;
        Color statusColor = {200, 200, 200, 255};
        int statusX = -1; // -1 means center
        int statusY = 350;
    } text;

    // Background
    struct {
        std::vector<std::string> imagePaths;
        float cycleTime = 2.0f;
        std::string musicPath;
    } background;

    // Tooltips
    struct {
        std::vector<std::string> messages;
        float cycleTime = 3.0f;
        int fontSize = 20;
        Color color = {180, 180, 180, 255};
        int x = -1; // -1 means center
        int y = 400;
    } tooltips;
};

namespace Elysium::Services {

class AssetService;

class LoadingService {
public:
    void Initialize();
    void Shutdown();

    void LoadConfig(const std::string& configPath);
    void Draw(int screenWidth, int screenHeight);

    bool IsLoading() const;
    float GetProgress() const;
    int GetTotalAssets() const;
    int GetLoadedAssets() const;

    void LoadAssets(const std::vector<Asset>& assets, AssetService& assetService);
    void ClearQueue();

    // Debug interface
    void OnDebugDraw();
    void ToggleVisibility();
    bool IsVisible() const { return debugVisible_; }

private:
    void LoadingThreadFunction();
    void DrawProgressBar(int screenWidth, int screenHeight);
    void DrawLoadingText(int screenWidth, int screenHeight);
    void DrawBackground(int screenWidth, int screenHeight);
    void DrawTooltips(int screenWidth, int screenHeight);
    std::string FormatStatusText(const std::string& template_str, int loaded, int total);

    std::vector<Asset> assetQueue_;
    AssetService* assetService_;

    std::atomic<bool> isLoading_{false};
    std::atomic<int> totalAssets_{0};
    std::atomic<int> loadedAssets_{0};

    std::thread loadingThread_;
    std::mutex queueMutex_;
    bool shouldExit_{false};

    LoadingConfig config_;
    std::vector<Texture2D> backgroundTextures_;
    Sound backgroundMusic_;
    float backgroundTimer_{0.0f};
    int currentBackgroundIndex_{0};
    bool musicLoaded_{false};

    float tooltipTimer_{0.0f};
    int currentTooltipIndex_{0};

    // Debug visibility
    bool debugVisible_{false};
};

} // namespace Elysium::Services

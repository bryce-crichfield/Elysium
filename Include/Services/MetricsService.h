#pragma once

#include "raylib.h"
#include <vector>
#include <chrono>
#include <string>

namespace Elysium::Services {

class MemoryTracker;

struct SystemMetrics {
    // Memory metrics (in bytes)
    size_t virtualMemorySize = 0;
    size_t physicalMemorySize = 0;
    size_t peakWorkingSet = 0;
    size_t workingSet = 0;
    
    // CPU metrics
    double cpuUsagePercent = 0.0;
    size_t threadCount = 0;
    
    // Process info
    long long processId = 0;
    std::string processName;
    
    // System info
    size_t totalSystemMemory = 0;
    size_t availableSystemMemory = 0;
};

class MetricsService {
public:
    MetricsService();
    ~MetricsService() = default;

    void Update(float deltaTime);
    void Draw();
    
    void SetVisible(bool visible) { isVisible_ = visible; }
    bool IsVisible() const { return isVisible_; }
    void ToggleVisibility() { isVisible_ = !isVisible_; }
    
    void RecordFrameTime(float frameTime);
    void RecordRenderTime(float renderTime);
    
    const SystemMetrics& GetSystemMetrics() const { return systemMetrics_; }

private:
    bool isVisible_;
    
    std::vector<float> frameTimeHistory_;
    std::vector<float> renderTimeHistory_;
    std::vector<float> memoryUsageHistory_;
    std::vector<float> cpuUsageHistory_;
    std::vector<float> heapUsageHistory_;
    
    static constexpr size_t MAX_HISTORY_SIZE = 120;
    static constexpr float GRAPH_HEIGHT = 80.0f;
    
    float maxFrameTime_;
    float maxRenderTime_;
    float avgFrameTime_;
    float avgRenderTime_;
    
    SystemMetrics systemMetrics_;
    std::chrono::steady_clock::time_point lastCpuTime_;
    size_t lastCpuTicks_;
    
    void UpdateAverages();
    void UpdateSystemMetrics();
    void DrawGraph(const char* label, const std::vector<float>& data, float maxValue, float graphHeight);
    void DrawSystemInfo();
    void DrawMemoryInfo();
    void DrawHeapInfo();
    
    // Platform-specific system info gathering
    void GatherLinuxSystemInfo();
    std::string FormatBytes(size_t bytes);
};

} // namespace Elysium::Services
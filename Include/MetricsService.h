#pragma once

#include "raylib.h"
#include <vector>
#include <chrono>

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

private:
    bool isVisible_;
    
    std::vector<float> frameTimeHistory_;
    std::vector<float> renderTimeHistory_;
    
    static constexpr size_t MAX_HISTORY_SIZE = 120;
    static constexpr float GRAPH_HEIGHT = 80.0f;
    
    float maxFrameTime_;
    float maxRenderTime_;
    float avgFrameTime_;
    float avgRenderTime_;
    
    void UpdateAverages();
    void DrawGraph(const char* label, const std::vector<float>& data, float maxValue, float graphHeight);
};
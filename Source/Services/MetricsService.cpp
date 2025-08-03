#include "Services/MetricsService.h"
#include "imgui.h"
#include <algorithm>
#include <numeric>

namespace Elysium::Services {

MetricsService::MetricsService() 
    : isVisible_(false), maxFrameTime_(0.0f), maxRenderTime_(0.0f), avgFrameTime_(0.0f), avgRenderTime_(0.0f) {
    frameTimeHistory_.reserve(MAX_HISTORY_SIZE);
    renderTimeHistory_.reserve(MAX_HISTORY_SIZE);
}

void MetricsService::Update(float deltaTime) {
    if (!isVisible_) return;
    
    UpdateAverages();
}

void MetricsService::Draw() {
    if (!isVisible_) return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Performance Metrics", &isVisible_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("FPS: %.1f", 1.0f / avgFrameTime_);
        ImGui::Text("Frame Time: %.3f ms (avg: %.3f ms)", 
                   frameTimeHistory_.empty() ? 0.0f : frameTimeHistory_.back() * 1000.0f,
                   avgFrameTime_ * 1000.0f);
        ImGui::Text("Render Time: %.3f ms (avg: %.3f ms)", 
                   renderTimeHistory_.empty() ? 0.0f : renderTimeHistory_.back() * 1000.0f,
                   avgRenderTime_ * 1000.0f);
        
        ImGui::Separator();
        
        if (!frameTimeHistory_.empty()) {
            DrawGraph("Frame Time (ms)", frameTimeHistory_, maxFrameTime_ * 1000.0f, GRAPH_HEIGHT);
        }
        
        if (!renderTimeHistory_.empty()) {
            DrawGraph("Render Time (ms)", renderTimeHistory_, maxRenderTime_ * 1000.0f, GRAPH_HEIGHT);
        }
    }
    ImGui::End();
}

void MetricsService::RecordFrameTime(float frameTime) {
    frameTimeHistory_.push_back(frameTime);
    if (frameTimeHistory_.size() > MAX_HISTORY_SIZE) {
        frameTimeHistory_.erase(frameTimeHistory_.begin());
    }
    maxFrameTime_ = std::max(maxFrameTime_, frameTime);
}

void MetricsService::RecordRenderTime(float renderTime) {
    renderTimeHistory_.push_back(renderTime);
    if (renderTimeHistory_.size() > MAX_HISTORY_SIZE) {
        renderTimeHistory_.erase(renderTimeHistory_.begin());
    }
    maxRenderTime_ = std::max(maxRenderTime_, renderTime);
}

void MetricsService::UpdateAverages() {
    if (!frameTimeHistory_.empty()) {
        avgFrameTime_ = std::accumulate(frameTimeHistory_.begin(), frameTimeHistory_.end(), 0.0f) / frameTimeHistory_.size();
    }
    
    if (!renderTimeHistory_.empty()) {
        avgRenderTime_ = std::accumulate(renderTimeHistory_.begin(), renderTimeHistory_.end(), 0.0f) / renderTimeHistory_.size();
    }
}

void MetricsService::DrawGraph(const char* label, const std::vector<float>& data, float maxValue, float graphHeight) {
    if (data.empty()) return;
    
    std::vector<float> displayData;
    displayData.reserve(data.size());
    
    for (float value : data) {
        displayData.push_back(value * 1000.0f);
    }
    
    ImGui::PlotLines(label, displayData.data(), displayData.size(), 0, nullptr, 0.0f, maxValue, ImVec2(0, graphHeight));
}

} // namespace Elysium::Services
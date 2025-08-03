#include "Services/MetricsService.h"
#include "Services/MemoryTracker.h"
#include "imgui.h"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>

namespace Elysium::Services {

MetricsService::MetricsService() 
    : isVisible_(false), maxFrameTime_(0.0f), maxRenderTime_(0.0f), avgFrameTime_(0.0f), avgRenderTime_(0.0f),
      lastCpuTicks_(0) {
    frameTimeHistory_.reserve(MAX_HISTORY_SIZE);
    renderTimeHistory_.reserve(MAX_HISTORY_SIZE);
    memoryUsageHistory_.reserve(MAX_HISTORY_SIZE);
    cpuUsageHistory_.reserve(MAX_HISTORY_SIZE);
    heapUsageHistory_.reserve(MAX_HISTORY_SIZE);
    
    lastCpuTime_ = std::chrono::steady_clock::now();
    systemMetrics_.processId = getpid();
    systemMetrics_.processName = "Elysium";
}

void MetricsService::Update(float deltaTime) {
    if (!isVisible_) return;
    
    UpdateAverages();
    UpdateSystemMetrics();
}

void MetricsService::Draw() {
    if (!isVisible_) return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Performance Metrics", &isVisible_, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginTabBar("MetricsTabs")) {
            if (ImGui::BeginTabItem("Performance")) {
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
                
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Memory")) {
                DrawMemoryInfo();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Heap")) {
                DrawHeapInfo();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("System")) {
                DrawSystemInfo();
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
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

void MetricsService::UpdateSystemMetrics() {
    GatherLinuxSystemInfo();
    
    // Record memory usage for history graph
    float memoryUsageMB = static_cast<float>(systemMetrics_.physicalMemorySize) / (1024.0f * 1024.0f);
    memoryUsageHistory_.push_back(memoryUsageMB);
    if (memoryUsageHistory_.size() > MAX_HISTORY_SIZE) {
        memoryUsageHistory_.erase(memoryUsageHistory_.begin());
    }
    
    // Record CPU usage for history graph
    cpuUsageHistory_.push_back(static_cast<float>(systemMetrics_.cpuUsagePercent));
    if (cpuUsageHistory_.size() > MAX_HISTORY_SIZE) {
        cpuUsageHistory_.erase(cpuUsageHistory_.begin());
    }
    
    // Record heap usage for history graph
    const MemoryTracker& tracker = MemoryTracker::GetInstance();
    float heapUsageMB = static_cast<float>(tracker.GetCurrentAllocated()) / (1024.0f * 1024.0f);
    heapUsageHistory_.push_back(heapUsageMB);
    if (heapUsageHistory_.size() > MAX_HISTORY_SIZE) {
        heapUsageHistory_.erase(heapUsageHistory_.begin());
    }
}

void MetricsService::DrawMemoryInfo() {
    ImGui::Text("Process Memory Usage");
    ImGui::Separator();
    
    ImGui::Text("Virtual Memory Size: %s", FormatBytes(systemMetrics_.virtualMemorySize).c_str());
    ImGui::Text("Physical Memory Size: %s", FormatBytes(systemMetrics_.physicalMemorySize).c_str());
    ImGui::Text("Working Set: %s", FormatBytes(systemMetrics_.workingSet).c_str());
    ImGui::Text("Peak Working Set: %s", FormatBytes(systemMetrics_.peakWorkingSet).c_str());
    
    ImGui::Separator();
    ImGui::Text("System Memory");
    ImGui::Text("Total System Memory: %s", FormatBytes(systemMetrics_.totalSystemMemory).c_str());
    ImGui::Text("Available Memory: %s", FormatBytes(systemMetrics_.availableSystemMemory).c_str());
    
    float usedPercent = systemMetrics_.totalSystemMemory > 0 ? 
        (float)(systemMetrics_.totalSystemMemory - systemMetrics_.availableSystemMemory) / systemMetrics_.totalSystemMemory * 100.0f : 0.0f;
    ImGui::Text("Memory Usage: %.1f%%", usedPercent);
    
    if (!memoryUsageHistory_.empty()) {
        float maxMemory = *std::max_element(memoryUsageHistory_.begin(), memoryUsageHistory_.end());
        std::vector<float> displayData = memoryUsageHistory_;
        ImGui::PlotLines("Memory Usage (MB)", displayData.data(), displayData.size(), 0, nullptr, 0.0f, maxMemory, ImVec2(0, GRAPH_HEIGHT));
    }
}

void MetricsService::DrawSystemInfo() {
    ImGui::Text("Process Information");
    ImGui::Separator();
    
    ImGui::Text("Process ID: %lld", systemMetrics_.processId);
    ImGui::Text("Process Name: %s", systemMetrics_.processName.c_str());
    ImGui::Text("Thread Count: %zu", systemMetrics_.threadCount);
    ImGui::Text("CPU Usage: %.2f%%", systemMetrics_.cpuUsagePercent);
    
    if (!cpuUsageHistory_.empty()) {
        std::vector<float> displayData = cpuUsageHistory_;
        ImGui::PlotLines("CPU Usage (%)", displayData.data(), displayData.size(), 0, nullptr, 0.0f, 100.0f, ImVec2(0, GRAPH_HEIGHT));
    }
}

void MetricsService::GatherLinuxSystemInfo() {
    // Get process memory info from /proc/self/status
    std::ifstream statusFile("/proc/self/status");
    std::string line;
    
    while (std::getline(statusFile, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        
        if (key == "VmSize:") {
            size_t value;
            iss >> value;
            systemMetrics_.virtualMemorySize = value * 1024; // Convert from KB to bytes
        } else if (key == "VmRSS:") {
            size_t value;
            iss >> value;
            systemMetrics_.physicalMemorySize = value * 1024; // Convert from KB to bytes
            systemMetrics_.workingSet = systemMetrics_.physicalMemorySize;
        } else if (key == "VmHWM:") {
            size_t value;
            iss >> value;
            systemMetrics_.peakWorkingSet = value * 1024; // Convert from KB to bytes
        } else if (key == "Threads:") {
            iss >> systemMetrics_.threadCount;
        }
    }
    statusFile.close();
    
    // Get CPU usage from /proc/self/stat
    std::ifstream statFile("/proc/self/stat");
    if (statFile.is_open()) {
        std::string statLine;
        std::getline(statFile, statLine);
        
        std::istringstream iss(statLine);
        std::string token;
        std::vector<std::string> tokens;
        
        while (iss >> token) {
            tokens.push_back(token);
        }
        
        if (tokens.size() > 13) {
            size_t utime = std::stoull(tokens[13]);  // User CPU time
            size_t stime = std::stoull(tokens[14]);  // System CPU time
            size_t totalTicks = utime + stime;
            
            auto currentTime = std::chrono::steady_clock::now();
            auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastCpuTime_);
            
            if (timeDiff.count() > 0 && lastCpuTicks_ > 0) {
                size_t tickDiff = totalTicks - lastCpuTicks_;
                double timeDiffSec = timeDiff.count() / 1000.0;
                systemMetrics_.cpuUsagePercent = (tickDiff / sysconf(_SC_CLK_TCK)) / timeDiffSec * 100.0;
            }
            
            lastCpuTicks_ = totalTicks;
            lastCpuTime_ = currentTime;
        }
        statFile.close();
    }
    
    // Get system memory info
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        systemMetrics_.totalSystemMemory = si.totalram * si.mem_unit;
        systemMetrics_.availableSystemMemory = si.freeram * si.mem_unit;
    }
}

void MetricsService::DrawHeapInfo() {
    const MemoryTracker& tracker = MemoryTracker::GetInstance();
    
    ImGui::Text("Tracked Heap Allocations");
    ImGui::Separator();
    
    ImGui::TextWrapped("Note: This tracks only explicit tracked allocations using TRACKED_MALLOC/FREE or TrackedAllocator. "
                      "Use these in your code for precise memory tracking.");
    
    ImGui::Separator();
    ImGui::Text("Current Allocated: %s", FormatBytes(tracker.GetCurrentAllocated()).c_str());
    ImGui::Text("Peak Allocated: %s", FormatBytes(tracker.GetPeakAllocated()).c_str());
    ImGui::Text("Total Allocated: %s", FormatBytes(tracker.GetTotalAllocated()).c_str());
    ImGui::Text("Total Deallocated: %s", FormatBytes(tracker.GetTotalDeallocated()).c_str());
    
    ImGui::Separator();
    ImGui::Text("Allocation Statistics");
    ImGui::Text("Total Allocations: %zu", tracker.GetAllocationCount());
    ImGui::Text("Total Deallocations: %zu", tracker.GetDeallocationCount());
    ImGui::Text("Active Allocations: %zu", tracker.GetAllocationCount() - tracker.GetDeallocationCount());
    
    float leakage = 0.0f;
    if (tracker.GetTotalDeallocated() > 0) {
        leakage = (float)(tracker.GetTotalAllocated() - tracker.GetTotalDeallocated()) / tracker.GetTotalAllocated() * 100.0f;
    }
    ImGui::Text("Potential Leakage: %.2f%%", leakage);
    
    ImGui::Separator();
    if (ImGui::Button("Reset Heap Statistics")) {
        const_cast<MemoryTracker&>(tracker).Reset();
    }
    
    ImGui::Separator();
    ImGui::Text("Usage Examples:");
    ImGui::BulletText("void* ptr = TRACKED_MALLOC(1024);");
    ImGui::BulletText("TRACKED_FREE(ptr, 1024);");
    ImGui::BulletText("std::vector<int, TrackedAllocator<int>> vec;");
    
    if (!heapUsageHistory_.empty()) {
        float maxHeap = *std::max_element(heapUsageHistory_.begin(), heapUsageHistory_.end());
        std::vector<float> displayData = heapUsageHistory_;
        ImGui::PlotLines("Tracked Heap Usage (MB)", displayData.data(), displayData.size(), 0, nullptr, 0.0f, maxHeap, ImVec2(0, GRAPH_HEIGHT));
    }
}

std::string MetricsService::FormatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unitIndex]);
    return std::string(buffer);
}

} // namespace Elysium::Services
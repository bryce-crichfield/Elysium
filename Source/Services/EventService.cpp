#include "Services/EventService.h"
#include "imgui.h"
#include "Common.h"

namespace Elysium::Services {

EventService::EventService() {
    name_ = "EventService";
}

void EventService::Initialize() {
    // Event service doesn't need specific initialization
    totalEventsProcessed_ = 0;
    eventCounts_.clear();

    isVisible_ = false;
}

void EventService::Shutdown() {
    // Clear all event listeners
    listeners.clear();
    eventCounts_.clear();
    totalEventsProcessed_ = 0;
}

void EventService::Update(float deltaTime) {
    Profile;
    // Event service doesn't need per-frame updates
}

void EventService::OnDebugDraw() {
    Profile;
        ImGui::Text("Total Events Processed: %zu", totalEventsProcessed_);
        ImGui::Text("Active Event Types: %zu", listeners.size());
        ImGui::Text("Event Type Counts: %zu", eventCounts_.size());

        ImGui::Separator();
        ImGui::Text("Event Statistics:");

        for (const auto& [typeIndex, count] : eventCounts_) {
            ImGui::Text("- %s: %zu events", typeIndex.name(), count);
        }

        if (ImGui::Button("Clear Statistics")) {
            eventCounts_.clear();
            totalEventsProcessed_ = 0;
        }
}

} // namespace Elysium::Services

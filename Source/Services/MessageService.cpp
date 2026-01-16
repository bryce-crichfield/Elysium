#pragma once

#include "Services/MessageService.h"
#include "Services/SceneService.h"
#include "Application.h"

namespace Elysium::Services {

MessageService::MessageService()
{

}

// Service interface
void MessageService::Initialize()
{

}

void MessageService::Shutdown() 
{

}

void MessageService::Update(float deltaTime) {
    auto messages = queue_.Swap();

    if (messages.empty()) return;

    auto &scenes = Application::GetInstance().GetService<SceneService>();

    for (auto &msg : messages) {
        scenes.OnMessage(*msg);
    }
}

void MessageService::ImGui() 
{

}

// Remove all subscriptions for an owner (call in OnExit/Shutdown)
void MessageService::UnsubscribeAll(void* owner)
{
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    // Iterate through all message types in the map
    for (auto& [type, entries] : handlers_) {
        // Remove all handlers belonging to this specific owner
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [owner](const HandlerEntry& entry) {
                    return entry.owner == owner;
                }),
            entries.end()
        );
    }
}
} // namespace Elysium::Services
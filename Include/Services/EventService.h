#pragma once

#include "../Scene.h"
#include <queue>

namespace Elysium::Services {

class EventService {
public:
    void QueueInputEvent(const InputEvent& event);
    void QueueNetworkEvent(const NetworkEvent& event);
    
    bool HasInputEvents() const;
    bool HasNetworkEvents() const;
    
    InputEvent GetNextInputEvent();
    NetworkEvent GetNextNetworkEvent();
    
    void Clear();
    void ClearInputEvents();
    void ClearNetworkEvents();

private:
    std::queue<InputEvent> inputEvents_;
    std::queue<NetworkEvent> networkEvents_;
};

} // namespace Elysium::Services
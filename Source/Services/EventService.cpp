#include "Services/EventService.h"

namespace Elysium::Services {

void EventService::Queue(const InputEvent& event) {
    inputEvents_.push(event);
}

void EventService::Queue(const NetworkEvent& event) {
    networkEvents_.push(event);
}

bool EventService::HasInputEvents() const {
    return !inputEvents_.empty();
}

bool EventService::HasNetworkEvents() const {
    return !networkEvents_.empty();
}

InputEvent EventService::GetNextInputEvent() {
    if (inputEvents_.empty()) {
        return {};
    }
    InputEvent event = inputEvents_.front();
    inputEvents_.pop();
    return event;
}

NetworkEvent EventService::GetNextNetworkEvent() {
    if (networkEvents_.empty()) {
        return {};
    }
    NetworkEvent event = networkEvents_.front();
    networkEvents_.pop();
    return event;
}

void EventService::Clear() {
    while (!inputEvents_.empty()) inputEvents_.pop();
    while (!networkEvents_.empty()) networkEvents_.pop();
}

void EventService::ClearInputEvents() {
    while (!inputEvents_.empty()) inputEvents_.pop();
}

void EventService::ClearNetworkEvents() {
    while (!networkEvents_.empty()) networkEvents_.pop();
}

} // namespace Elysium::Services
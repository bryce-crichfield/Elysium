#pragma once

#include <algorithm>
#include <functional>
#include <vector>

namespace Elysium {

template <typename... Args>
class Signal {
public:
    using SlotType = std::function<void(Args...)>;
    using ConnectionID = size_t;

    // Connect and return an ID so we can disconnect later
    ConnectionID Connect(SlotType slot) {
        ConnectionID id = next_id_++;
        slots_.emplace_back(id, std::move(slot));
        return id;
    }

    void Disconnect(ConnectionID id) {
        slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
            [id](const auto& pair) { return pair.first == id; }), 
            slots_.end());
    }

    void Emit(Args... args) {
        // Copy or use an index-based loop to prevent crashes 
        // if a slot calls Disconnect() or destroys an entity during the loop.
        for (size_t i = 0; i < slots_.size(); ++i) {
            slots_[i].second(args...);
        }
    }

    void Clear() {
        slots_.clear();
    }

private:
    size_t next_id_ = 0;
    std::vector<std::pair<ConnectionID, SlotType>> slots_;
};
};
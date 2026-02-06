#pragma once

#include <memory>
#include <mutex>
#include <typeinfo>
#include <vector>

namespace Elysium {

// Base message class for async inter-thread communication
class Message {
   public:
    virtual ~Message() = default;

    // Template methods for clean type checking (mirrors Event pattern)
    template <typename T>
    bool Is() const {
        return typeid(*this) == typeid(T);
    }

    template <typename T>
    T* As() {
        if (Is<T>()) {
            return static_cast<T*>(this);
        }
        return nullptr;
    }

    template <typename T>
    const T* As() const {
        if (Is<T>()) {
            return static_cast<const T*>(this);
        }
        return nullptr;
    }
};

class MessageQueue {
   public:
    void Push(std::unique_ptr<Message> msg);
    std::vector<std::unique_ptr<Message>> Swap();

    std::mutex& GetMutex() { return mutex_; }

   private:
    std::mutex mutex_;
    std::vector<std::unique_ptr<Message>> bufferA_;
    std::vector<std::unique_ptr<Message>> bufferB_;
    std::vector<std::unique_ptr<Message>>* writeBuffer_ = &bufferA_;
};

class IMessageListener {
public:
    virtual ~IMessageListener() = default;
    virtual void OnMessage(Message& message) = 0;
};

}  // namespace Elysium

#pragma once

#include <typeinfo>
#include "raylib.h"

namespace Elysium {

// Base event class
class Event {
   public:
    virtual ~Event() = default;

    bool handled = false;

    // Template methods for clean type checking
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

// Mouse events - coordinates are in framebuffer space (already transformed)
class MouseButtonPressedEvent : public Event {
   public:
    MouseButtonPressedEvent(int button, Vector2 position)
        : button_(button), position_(position) {}

    int GetButton() const { return button_; }
    Vector2 GetPosition() const { return position_; }

   private:
    int button_;
    Vector2 position_;
};

class MouseButtonReleasedEvent : public Event {
   public:
    MouseButtonReleasedEvent(int button, Vector2 position)
        : button_(button), position_(position) {}

    int GetButton() const { return button_; }
    Vector2 GetPosition() const { return position_; }

   private:
    int button_;
    Vector2 position_;
};

class MouseMovedEvent : public Event {
   public:
    MouseMovedEvent(Vector2 position, Vector2 delta)
        : position_(position), delta_(delta) {}

    Vector2 GetPosition() const { return position_; }
    Vector2 GetDelta() const { return delta_; }

   private:
    Vector2 position_;
    Vector2 delta_;
};

class MouseWheelEvent : public Event {
   public:
    MouseWheelEvent(float wheelMove, Vector2 position)
        : wheelMove_(wheelMove), position_(position) {}

    float GetWheelMove() const { return wheelMove_; }
    Vector2 GetPosition() const { return position_; }

   private:
    float wheelMove_;
    Vector2 position_;
};

// Keyboard events
class KeyPressedEvent : public Event {
   public:
    KeyPressedEvent(int key) : key_(key) {}

    int GetKey() const { return key_; }

   private:
    int key_;
};

class KeyReleasedEvent : public Event {
   public:
    KeyReleasedEvent(int key) : key_(key) {}

    int GetKey() const { return key_; }

   private:
    int key_;
};

}  // namespace Elysium

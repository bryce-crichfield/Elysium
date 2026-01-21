#pragma once

#include <cstdint>
#include <memory>
#include "Event.h"
#include "ByteBuffer.h"

namespace Elysium::Network {

/**
 * Network Input Types - mirrors local Event types
 */
enum class NetworkInputType : uint8_t {
    MouseButtonPressed = 1,
    MouseButtonReleased = 2,
    MouseMoved = 3,
    MouseWheel = 4,
    KeyPressed = 5,
    KeyReleased = 6,
};

/**
 * NetworkInput - Serializable input event for network transmission
 *
 * Raw input forwarding approach - sends mouse/key events directly
 * rather than game-specific commands. This keeps the network layer
 * decoupled from game logic.
 */
struct NetworkInput {
    NetworkInputType type;
    uint32_t sequence;  // For ordering and acknowledgment

    // Union of input data based on type
    union {
        struct {
            int32_t button;
            float x;
            float y;
        } mouseButton;  // MouseButtonPressed, MouseButtonReleased

        struct {
            float x;
            float y;
            float dx;
            float dy;
        } mouseMove;  // MouseMoved

        struct {
            float delta;
            float x;
            float y;
        } mouseWheel;  // MouseWheel

        struct {
            int32_t key;
        } keyboard;  // KeyPressed, KeyReleased
    };

    NetworkInput() : type(NetworkInputType::KeyPressed), sequence(0) {
        std::memset(&keyboard, 0, sizeof(keyboard));
    }

    // === Serialization ===

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU8(static_cast<uint8_t>(type));
        buffer.WriteU32(sequence);

        switch (type) {
            case NetworkInputType::MouseButtonPressed:
            case NetworkInputType::MouseButtonReleased:
                buffer.WriteI32(mouseButton.button);
                buffer.WriteFloat(mouseButton.x);
                buffer.WriteFloat(mouseButton.y);
                break;

            case NetworkInputType::MouseMoved:
                buffer.WriteFloat(mouseMove.x);
                buffer.WriteFloat(mouseMove.y);
                buffer.WriteFloat(mouseMove.dx);
                buffer.WriteFloat(mouseMove.dy);
                break;

            case NetworkInputType::MouseWheel:
                buffer.WriteFloat(mouseWheel.delta);
                buffer.WriteFloat(mouseWheel.x);
                buffer.WriteFloat(mouseWheel.y);
                break;

            case NetworkInputType::KeyPressed:
            case NetworkInputType::KeyReleased:
                buffer.WriteI32(keyboard.key);
                break;
        }
    }

    void Read(ByteBuffer& buffer) {
        type = static_cast<NetworkInputType>(buffer.ReadU8());
        sequence = buffer.ReadU32();

        switch (type) {
            case NetworkInputType::MouseButtonPressed:
            case NetworkInputType::MouseButtonReleased:
                mouseButton.button = buffer.ReadI32();
                mouseButton.x = buffer.ReadFloat();
                mouseButton.y = buffer.ReadFloat();
                break;

            case NetworkInputType::MouseMoved:
                mouseMove.x = buffer.ReadFloat();
                mouseMove.y = buffer.ReadFloat();
                mouseMove.dx = buffer.ReadFloat();
                mouseMove.dy = buffer.ReadFloat();
                break;

            case NetworkInputType::MouseWheel:
                mouseWheel.delta = buffer.ReadFloat();
                mouseWheel.x = buffer.ReadFloat();
                mouseWheel.y = buffer.ReadFloat();
                break;

            case NetworkInputType::KeyPressed:
            case NetworkInputType::KeyReleased:
                keyboard.key = buffer.ReadI32();
                break;
        }
    }

    // === Factory Methods - Create from local Events ===

    static NetworkInput FromMouseButtonPressed(const MouseButtonPressedEvent& event, uint32_t seq) {
        NetworkInput input;
        input.type = NetworkInputType::MouseButtonPressed;
        input.sequence = seq;
        input.mouseButton.button = event.GetButton();
        input.mouseButton.x = event.GetPosition().x;
        input.mouseButton.y = event.GetPosition().y;
        return input;
    }

    static NetworkInput FromMouseButtonReleased(const MouseButtonReleasedEvent& event, uint32_t seq) {
        NetworkInput input;
        input.type = NetworkInputType::MouseButtonReleased;
        input.sequence = seq;
        input.mouseButton.button = event.GetButton();
        input.mouseButton.x = event.GetPosition().x;
        input.mouseButton.y = event.GetPosition().y;
        return input;
    }

    static NetworkInput FromMouseMoved(const MouseMovedEvent& event, uint32_t seq) {
        NetworkInput input;
        input.type = NetworkInputType::MouseMoved;
        input.sequence = seq;
        input.mouseMove.x = event.GetPosition().x;
        input.mouseMove.y = event.GetPosition().y;
        input.mouseMove.dx = event.GetDelta().x;
        input.mouseMove.dy = event.GetDelta().y;
        return input;
    }

    static NetworkInput FromMouseWheel(const MouseWheelEvent& event, uint32_t seq) {
        NetworkInput input;
        input.type = NetworkInputType::MouseWheel;
        input.sequence = seq;
        input.mouseWheel.delta = event.GetWheelMove();
        input.mouseWheel.x = event.GetPosition().x;
        input.mouseWheel.y = event.GetPosition().y;
        return input;
    }

    static NetworkInput FromKeyPressed(const KeyPressedEvent& event, uint32_t seq) {
        NetworkInput input;
        input.type = NetworkInputType::KeyPressed;
        input.sequence = seq;
        input.keyboard.key = event.GetKey();
        return input;
    }

    static NetworkInput FromKeyReleased(const KeyReleasedEvent& event, uint32_t seq) {
        NetworkInput input;
        input.type = NetworkInputType::KeyReleased;
        input.sequence = seq;
        input.keyboard.key = event.GetKey();
        return input;
    }

    // === Conversion to local Events ===

    std::unique_ptr<Event> ToEvent() const {
        switch (type) {
            case NetworkInputType::MouseButtonPressed:
                return std::make_unique<MouseButtonPressedEvent>(
                    mouseButton.button,
                    Vector2{mouseButton.x, mouseButton.y});

            case NetworkInputType::MouseButtonReleased:
                return std::make_unique<MouseButtonReleasedEvent>(
                    mouseButton.button,
                    Vector2{mouseButton.x, mouseButton.y});

            case NetworkInputType::MouseMoved:
                return std::make_unique<MouseMovedEvent>(
                    Vector2{mouseMove.x, mouseMove.y},
                    Vector2{mouseMove.dx, mouseMove.dy});

            case NetworkInputType::MouseWheel:
                return std::make_unique<MouseWheelEvent>(
                    mouseWheel.delta,
                    Vector2{mouseWheel.x, mouseWheel.y});

            case NetworkInputType::KeyPressed:
                return std::make_unique<KeyPressedEvent>(keyboard.key);

            case NetworkInputType::KeyReleased:
                return std::make_unique<KeyReleasedEvent>(keyboard.key);
        }
        return nullptr;
    }
};

/**
 * Input Packet - Batches multiple inputs for efficient transmission
 * Client -> Server
 */
struct InputPacketData {
    uint32_t clientTick;  // Client's local tick when inputs were generated
    uint8_t inputCount;   // Number of inputs in this packet

    void Write(ByteBuffer& buffer) const {
        buffer.WriteU32(clientTick);
        buffer.WriteU8(inputCount);
    }

    void Read(ByteBuffer& buffer) {
        clientTick = buffer.ReadU32();
        inputCount = buffer.ReadU8();
    }
};

}  // namespace Elysium::Network

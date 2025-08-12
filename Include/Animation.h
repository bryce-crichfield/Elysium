#pragma once

#include "System.h"
#include "raylib.h"
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

namespace Elysium
{
struct Action;

struct MoveTo
{
    Vector2 target;
    Vector2 targetWorldPos;
    Vector2 startPos;
    float duration, elapsed = 0;
    bool isStarted = false;
};

struct MoveBy
{
    Vector2 delta;
    Vector2 startPos;
    float duration, elapsed = 0;
    bool isStarted = false;
};

struct PlayFrames
{
    int start, end;
    float duration, elapsed = 0;
    int currentFrame = 0;
};

struct SwitchMarker
{
    std::string sheetName;  // e.g., "idle", "walk"
    std::string markerName; // e.g., "down", "up"
};

struct Wait
{
    float duration, elapsed = 0;
};

struct Callback
{
    std::function<void()> function;
};

struct Sequence
{
    std::vector<std::shared_ptr<Action>> actions;
    size_t index = 0;
};

struct Parallel
{
    std::vector<std::shared_ptr<Action>> actions;
    std::vector<bool> isCompletedFlags;
};

struct Repeat
{
    int times, count = 0;
    std::shared_ptr<Action> actionTemplate;
    std::shared_ptr<Action> current = nullptr;
};

struct Loop
{
    std::function<bool()> condition;
    std::shared_ptr<Action> actionTemplate;
    std::shared_ptr<Action> current = nullptr;
};

using ActionVariant = std::variant<MoveTo, MoveBy, PlayFrames, SwitchMarker, Wait, Callback, Sequence, Parallel, Repeat, Loop>;

struct Action
{
    ActionVariant data;

    template <typename T>
    Action(T&& actionData) : data(std::forward<T>(actionData))
    {
    }
};

namespace Fx
{
inline auto MakeAction = [](auto&& data) { return std::make_shared<Action>(std::forward<decltype(data)>(data)); };

inline auto MoveTo = [](float x, float y, float duration = 1.0f) {
    return MakeAction(Elysium::MoveTo{Vector2{x, y}, Vector2{}, Vector2{}, duration});
};

inline auto MoveBy = [](float x, float y, float duration = 1.0f) {
    return MakeAction(Elysium::MoveBy{Vector2{x, y}, Vector2{}, duration});
};

inline auto PlayFrames = [](int start, int end, float duration = 1.0f) {
    return MakeAction(Elysium::PlayFrames{start, end, duration});
};

inline auto SwitchMarker = [](const std::string& sheetName, const std::string& markerName) {
    return MakeAction(Elysium::SwitchMarker{sheetName, markerName});
};

inline auto Wait = [](float duration) { return MakeAction(Elysium::Wait{duration}); };

inline auto Callback = [](std::function<void()> fn) { return MakeAction(Elysium::Callback{fn}); };

template <typename... Args>
inline auto Sequence(Args&&... actions)
{
    return MakeAction(Elysium::Sequence{std::vector<std::shared_ptr<Action>>{actions...}});
}

template <typename... Args>
inline auto Parallel(Args&&... actions)
{
    return MakeAction(Elysium::Parallel{std::vector<std::shared_ptr<Action>>{actions...}});
}

inline auto Repeat = [](int times, std::shared_ptr<Action> action) {
    return MakeAction(Elysium::Repeat{times, 0, action});
};

inline auto Loop = [](std::function<bool()> condition, std::shared_ptr<Action> action) {
    return MakeAction(Elysium::Loop{condition, action});
};
} // namespace Fx

} // namespace Elysium

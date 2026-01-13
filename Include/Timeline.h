#pragma once

#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "Entity.h"
#include "raylib.h"

namespace Elysium {

// Forward declarations
class World;

enum class Interpolation {
    Step,        // Instant jump to value at keyframe time
    Linear       // Linear interpolation (lerp)
};

template<typename T>
struct Keyframe {
    float time;
    T value;
    Interpolation interpolation = Interpolation::Linear;

    Keyframe() = default;
    Keyframe(float t, T val, Interpolation interp = Interpolation::Linear)
        : time(t), value(val), interpolation(interp) {}
};

class TrackBase {
protected:
    std::string name_;
    Entity targetEntity_;
    std::string componentName_;
    std::string propertyName_;
    bool enabled_ = true;

public:
    TrackBase(const std::string& name, Entity target, const std::string& component, const std::string& property)
        : name_(name), targetEntity_(target), componentName_(component), propertyName_(property) {}

    virtual ~TrackBase() = default;

    const std::string& GetName() const { return name_; }
    Entity GetTargetEntity() const { return targetEntity_; }
    const std::string& GetComponentName() const { return componentName_; }
    const std::string& GetPropertyName() const { return propertyName_; }
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    virtual float GetDuration() const = 0;
    virtual void ApplyAtTime(float time, World* world) = 0;
};

template <typename T>
using PropertySetter = std::function<void(Entity, World*, const T&)>;

template<typename T>
class Track : public TrackBase {
private:
    std::vector<Keyframe<T>> keyframes_;
    PropertySetter<T> setterFunc_;

public:
    Track(const std::string& name, Entity target, const std::string& component, const std::string& property)
        : TrackBase(name, target, component, property) {}

    // Add keyframe (maintains sorted order by time)
    void AddKeyframe(float time, T value, Interpolation interp = Interpolation::Linear) {
        Keyframe<T> kf(time, value, interp);

        // Find insertion point to maintain sorted order
        auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), kf,
            [](const Keyframe<T>& a, const Keyframe<T>& b) { return a.time < b.time; });

        keyframes_.insert(it, kf);
    }

    // Remove keyframe at index
    void RemoveKeyframe(size_t index) {
        if (index < keyframes_.size()) {
            keyframes_.erase(keyframes_.begin() + index);
        }
    }

    // Clear all keyframes
    void Clear() {
        keyframes_.clear();
    }

    // Get interpolated value at specific time
    T GetValueAtTime(float time) const {
        if (keyframes_.empty()) {
            return T{};
        }

        // Before first keyframe
        if (time <= keyframes_.front().time) {
            return keyframes_.front().value;
        }

        // After last keyframe
        if (time >= keyframes_.back().time) {
            return keyframes_.back().value;
        }

        // Find surrounding keyframes
        for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
            if (time >= keyframes_[i].time && time <= keyframes_[i + 1].time) {
                const auto& kf1 = keyframes_[i];
                const auto& kf2 = keyframes_[i + 1];

                if (kf1.interpolation == Interpolation::Step) {
                    return kf1.value;
                }

                // Linear interpolation
                float t = (time - kf1.time) / (kf2.time - kf1.time);
                return Lerp(kf1.value, kf2.value, t);
            }
        }

        return keyframes_.back().value;
    }

    float GetDuration() const override {
        return keyframes_.empty() ? 0.0f : keyframes_.back().time;
    }

    void ApplyAtTime(float time, World* world) override {
        if (!enabled_ || !setterFunc_) return;
        T value = GetValueAtTime(time);
        setterFunc_(targetEntity_, world, value);
    }

    // Set the function that applies values to components
    void SetPropertySetter(PropertySetter<T> setter) {
        setterFunc_ = setter;
    }

    // Access keyframes for GUI editing
    std::vector<Keyframe<T>>& GetKeyframes() { return keyframes_; }
    const std::vector<Keyframe<T>>& GetKeyframes() const { return keyframes_; }

private:
    // Lerp specializations
    static float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    static Vector2 Lerp(const Vector2& a, const Vector2& b, float t) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
    }
};

enum class TimelineState {
    Stopped,
    Playing,
    Paused,
    Finished
};

struct TimelineAction {
    float time;
    std::function<void(World*)> callback;
    bool triggered = false;
    std::string description;

    TimelineAction(float t, std::function<void(World*)> cb, const std::string& desc = "")
        : time(t), callback(cb), description(desc) {}
};

class Timeline {
private:
    std::string name_;
    TimelineState state_ = TimelineState::Stopped;
    float currentTime_ = 0.0f;
    float playbackSpeed_ = 1.0f;
    bool loop_ = false;

    std::vector<std::unique_ptr<TrackBase>> tracks_;
    std::vector<TimelineAction> actions_;

    mutable float cachedDuration_ = -1.0f;
    float manualDuration_ = 0.0f;  // User-set duration override

public:
    Timeline(const std::string& name = "Untitled") : name_(name) {}

    void Play() {
        state_ = TimelineState::Playing;
        // Reset action triggers
        for (auto& action : actions_) {
            action.triggered = false;
        }
    }

    void Pause() {
        if (state_ == TimelineState::Playing) {
            state_ = TimelineState::Paused;
        }
    }

    void Stop() {
        state_ = TimelineState::Stopped;
        currentTime_ = 0.0f;
        // Reset action triggers
        for (auto& action : actions_) {
            action.triggered = false;
        }
    }

    void Seek(float time) {
        currentTime_ = time;
        // Reset action triggers for actions before this time
        for (auto& action : actions_) {
            if (action.time < currentTime_) {
                action.triggered = true;
            } else {
                action.triggered = false;
            }
        }
    }

    void SetLoop(bool loop) { loop_ = loop; }
    void SetPlaybackSpeed(float speed) { playbackSpeed_ = speed; }

    TimelineState GetState() const { return state_; }
    bool IsPlaying() const { return state_ == TimelineState::Playing; }
    bool IsPaused() const { return state_ == TimelineState::Paused; }
    bool IsStopped() const { return state_ == TimelineState::Stopped; }
    bool IsFinished() const { return state_ == TimelineState::Finished; }

    float GetCurrentTime() const { return currentTime_; }
    float GetPlaybackSpeed() const { return playbackSpeed_; }
    bool IsLooping() const { return loop_; }

    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    float GetDuration() const {
        if (cachedDuration_ < 0.0f) {
            cachedDuration_ = 0.0f;
            for (const auto& track : tracks_) {
                cachedDuration_ = std::max(cachedDuration_, track->GetDuration());
            }
            for (const auto& action : actions_) {
                cachedDuration_ = std::max(cachedDuration_, action.time);
            }
        }
        // Use manual duration if set and greater than calculated
        return std::max(cachedDuration_, manualDuration_);
    }

    void SetDuration(float duration) {
        manualDuration_ = duration;
    }

    void Update(float deltaTime, World* world) {
        if (state_ != TimelineState::Playing) return;

        currentTime_ += deltaTime * playbackSpeed_;
        float duration = GetDuration();

        // Check if finished
        if (currentTime_ >= duration && duration > 0.0f) {
            if (loop_) {
                currentTime_ = 0.0f;
                // Reset action triggers
                for (auto& action : actions_) {
                    action.triggered = false;
                }
            } else {
                currentTime_ = duration;
                state_ = TimelineState::Finished;
            }
        }

        // Apply all tracks
        for (auto& track : tracks_) {
            track->ApplyAtTime(currentTime_, world);
        }

        // Trigger actions
        for (auto& action : actions_) {
            if (!action.triggered && currentTime_ >= action.time) {
                action.callback(world);
                action.triggered = true;
            }
        }
    }

    template<typename T>
    Track<T>* AddTrack(const std::string& name, Entity entity, const std::string& component, const std::string& property) {
        auto track = std::make_unique<Track<T>>(name, entity, component, property);
        Track<T>* ptr = track.get();
        tracks_.push_back(std::move(track));
        cachedDuration_ = -1.0f; // Invalidate cache
        return ptr;
    }

    void RemoveTrack(const std::string& name) {
        tracks_.erase(
            std::remove_if(tracks_.begin(), tracks_.end(),
                [&name](const std::unique_ptr<TrackBase>& track) {
                    return track->GetName() == name;
                }),
            tracks_.end()
        );
        cachedDuration_ = -1.0f;
    }

    TrackBase* GetTrack(const std::string& name) {
        for (auto& track : tracks_) {
            if (track->GetName() == name) {
                return track.get();
            }
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<TrackBase>>& GetTracks() const { return tracks_; }

    void AddAction(float time, std::function<void(World*)> callback, const std::string& description = "") {
        actions_.emplace_back(time, callback, description);
        cachedDuration_ = -1.0f;
    }

    const std::vector<TimelineAction>& GetActions() const { return actions_; }
};

} // namespace Elysium

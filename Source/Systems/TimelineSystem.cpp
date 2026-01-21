#include "Systems/TimelineSystem.h"
#include "Core/Component.h"
#include "Core/Scene.h"

namespace Elysium::Systems {

TimelineSystem::TimelineSystem(Context context) : System(context) {
    // Register PositionComponent.x
    RegisterProperty<PositionComponent, float>(
        "PositionComponent", "x",
        [](PositionComponent& c, const float& v) { c.x = v; });

    // Register PositionComponent.y
    RegisterProperty<PositionComponent, float>(
        "PositionComponent", "y",
        [](PositionComponent& c, const float& v) { c.y = v; });
}

void TimelineSystem::Update(float deltaTime) {
    // Get all timelines from scene and update them
    const auto& timelines = scene->GetTimelines();
    for (const auto& timeline : timelines) {
        // Bind accessors to tracks if not already bound
        for (const auto& trackPtr : timeline->GetTracks()) {
            BindTrackAccessor(trackPtr.get());
        }

        // Update the timeline
        timeline->Update(deltaTime, world);
    }
}

void TimelineSystem::BindTrackAccessor(TrackBase* track) {
    // Find matching accessor for this track
    for (auto& accessor : accessors_) {
        if (accessor.componentName == track->GetComponentName() &&
            accessor.propertyName == track->GetPropertyName()) {
            // Check if entity has the component
            if (!accessor.hasComponentFunc(track->GetTargetEntity(), world)) {
                // Log warning or skip
                return;
            }

            // Bind the accessor based on track type
            // For now we only support float tracks
            if (accessor.valueType == std::type_index(typeid(float))) {
                auto* floatTrack = dynamic_cast<Track<float>*>(track);
                if (floatTrack) {
                    floatTrack->SetPropertySetter(
                        [accessor](Entity entity, World* world, const float& value) {
                            accessor.applyFunc(entity, world, &value);
                        });
                }
            }
            return;
        }
    }
}

std::vector<std::string> TimelineSystem::GetPropertiesForComponent(const std::string& componentName) const {
    std::vector<std::string> properties;
    for (const auto& accessor : accessors_) {
        if (accessor.componentName == componentName) {
            properties.push_back(accessor.propertyName);
        }
    }
    return properties;
}

}  // namespace Elysium::Systems

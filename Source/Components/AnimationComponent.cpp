#include "Components/AnimationComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    AnimationComponent::AnimationComponent(std::string marker) : marker(marker) {}

    void AnimationComponent::LoadXml(AnimationComponent& c, tinyxml2::XMLElement* el) {
        // No attrs loaded in original
    }

    void AnimationComponent::Inspect(AnimationComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        static char markerBuffer[256];
        strncpy(markerBuffer, c.marker.c_str(), sizeof(markerBuffer) - 1);
        markerBuffer[sizeof(markerBuffer) - 1] = '\0';

        Label("Marker: ");
        if (ImGui::InputText("##Marker", markerBuffer, sizeof(markerBuffer))) {
            c.marker = std::string(markerBuffer);
        }

        Label("Current Frame: ");
        ImGui::DragInt("##CurrentFrame", &c.currentFrame, 1.0f, 0);
        Label("Start Frame: ");
        ImGui::DragInt("##StartFrame", &c.start, 1.0f, 0);
        Label("End Frame: ");
        ImGui::DragInt("##EndFrame", &c.end, 1.0f, 0);
        Label("Duration: ");
        ImGui::DragFloat("##FrameDuration", &c.frameDuration, 0.01f, 0.0f, 10.0f);
        Label("Elapsed: ");
        ImGui::DragFloat("##Elapsed", &c.elapsed, 0.01f, 0.0f);
        Label("Loop: ");
        ImGui::Checkbox("##Loop", &c.loop);
    }

    void AnimationComponent::BindLua(sol::usertype<AnimationComponent>& ut) {
        ut["marker"] = &AnimationComponent::marker;
        ut["currentFrame"] = &AnimationComponent::currentFrame;
        ut["start"] = &AnimationComponent::start;
        ut["end"] = &AnimationComponent::end;
        ut["frameDuration"] = &AnimationComponent::frameDuration;
        ut["elapsed"] = &AnimationComponent::elapsed;
        ut["loop"] = &AnimationComponent::loop;
    }

    void AnimationComponent::SetFromLua(AnimationComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.marker = t.get_or("marker", c.marker);
            c.currentFrame = t.get_or("currentFrame", c.currentFrame);
            c.start = t.get_or("start", c.start);
            c.end = t.get_or("end", c.end);
            c.frameDuration = t.get_or("frameDuration", c.frameDuration);
            c.loop = t.get_or("loop", c.loop);
        }
    }

    REGISTER_COMPONENT(AnimationComponent);
}

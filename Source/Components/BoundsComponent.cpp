#include "Components/BoundsComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "imgui.h"

namespace Elysium {
    void BoundsComponent::LoadXml(BoundsComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        // Computed component
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        return Colors::White;
    }

    void BoundsComponent::Inspect(BoundsComponent& c, Entity e, ServiceLocator& services) {
        PropertyLabel("X");
        ImGui::Text("%.1f", c.bounds.x);
        PropertyLabel("Y");
        ImGui::Text("%.1f", c.bounds.y);
        PropertyLabel("Width");
        ImGui::Text("%.1f", c.bounds.width);
        PropertyLabel("Height");
        ImGui::Text("%.1f", c.bounds.height);

        PropertyLabel("Is Dragging");
        ImGui::Checkbox("##IsDragging", &c.isDragging);

        float color[4] = {c.debugColor.r / 255.0f, c.debugColor.g / 255.0f,
                          c.debugColor.b / 255.0f, c.debugColor.a / 255.0f};
        PropertyLabel("Debug Color");
        if (ImGui::ColorEdit4("##DebugColor", color)) {
            c.debugColor = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255),
                                 (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255)};
        }

        ImGui::Spacing();
        MutedText("Bounds are computed automatically by RenderSystem.");
    }

    void BoundsComponent::BindLua(sol::usertype<BoundsComponent>& ut) {
        ut["bounds"] = &BoundsComponent::bounds;
        ut["isDragging"] = &BoundsComponent::isDragging;
        ut["debugColor"] = sol::property(
            [](BoundsComponent& b) { return b.debugColor; },
            [](BoundsComponent& b, sol::object v) { b.debugColor = ObjectToColor(v); });
    }

    void BoundsComponent::SetFromLua(BoundsComponent& c, sol::object v) {
        // Read-only usually? Or limited set.
    }

    REGISTER_COMPONENT(BoundsComponent);
}

#include "Components/TransformComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    TransformComponent::TransformComponent(float x, float y)
        : localX(x), localY(y), worldX(x), worldY(y) {}

    void TransformComponent::LoadXml(TransformComponent& c, tinyxml2::XMLElement* el) {
        c.localX = el->FloatAttribute("x", 0.0f);
        c.localY = el->FloatAttribute("y", 0.0f);
        c.localScaleX = el->FloatAttribute("scaleX", 1.0f);
        c.localScaleY = el->FloatAttribute("scaleY", 1.0f);
        c.localRotation = el->FloatAttribute("rotation", 0.0f);

        // Seed the world cache so a same-frame read before TransformSystem's
        // first tick (e.g. immediately after load) sees a sane value.
        c.worldX = c.localX;
        c.worldY = c.localY;
        c.worldScaleX = c.localScaleX;
        c.worldScaleY = c.localScaleY;
        c.worldRotation = c.localRotation;
    }

    void TransformComponent::SaveXml(const TransformComponent& c, XMLBuilder& builder) {
        auto element = builder.AddElement("TransformComponent")
            .SetAttribute("x", c.localX)
            .SetAttribute("y", c.localY);

        if (c.localScaleX != 1.0f) element.SetAttribute("scaleX", c.localScaleX);
        if (c.localScaleY != 1.0f) element.SetAttribute("scaleY", c.localScaleY);
        if (c.localRotation != 0.0f) element.SetAttribute("rotation", c.localRotation);
    }

    void TransformComponent::Inspect(TransformComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        ImGui::TextDisabled("Local");

        Label("X: ");
        std::string localXId = "##TransformLocalX_" + std::to_string(e);
        ImGui::DragFloat(localXId.c_str(), &c.localX, 1.0f);

        Label("Y: ");
        std::string localYId = "##TransformLocalY_" + std::to_string(e);
        ImGui::DragFloat(localYId.c_str(), &c.localY, 1.0f);

        Label("Scale X: ");
        std::string localScaleXId = "##TransformLocalScaleX_" + std::to_string(e);
        ImGui::DragFloat(localScaleXId.c_str(), &c.localScaleX, 0.01f, 0.0f);

        Label("Scale Y: ");
        std::string localScaleYId = "##TransformLocalScaleY_" + std::to_string(e);
        ImGui::DragFloat(localScaleYId.c_str(), &c.localScaleY, 0.01f, 0.0f);

        Label("Rotation: ");
        std::string localRotationId = "##TransformLocalRotation_" + std::to_string(e);
        ImGui::DragFloat(localRotationId.c_str(), &c.localRotation, 1.0f);

        ImGui::Spacing();
        ImGui::TextDisabled("World (read-only)");

        ImGui::BeginDisabled();

        Label("X: ");
        std::string worldXId = "##TransformWorldX_" + std::to_string(e);
        ImGui::DragFloat(worldXId.c_str(), &c.worldX, 1.0f);

        Label("Y: ");
        std::string worldYId = "##TransformWorldY_" + std::to_string(e);
        ImGui::DragFloat(worldYId.c_str(), &c.worldY, 1.0f);

        Label("Scale X: ");
        std::string worldScaleXId = "##TransformWorldScaleX_" + std::to_string(e);
        ImGui::DragFloat(worldScaleXId.c_str(), &c.worldScaleX, 0.01f, 0.0f);

        Label("Scale Y: ");
        std::string worldScaleYId = "##TransformWorldScaleY_" + std::to_string(e);
        ImGui::DragFloat(worldScaleYId.c_str(), &c.worldScaleY, 0.01f, 0.0f);

        Label("Rotation: ");
        std::string worldRotationId = "##TransformWorldRotation_" + std::to_string(e);
        ImGui::DragFloat(worldRotationId.c_str(), &c.worldRotation, 1.0f);

        ImGui::EndDisabled();
    }

    void TransformComponent::BindLua(sol::usertype<TransformComponent>& ut) {
        ut["localX"] = &TransformComponent::localX;
        ut["localY"] = &TransformComponent::localY;
        ut["localScaleX"] = &TransformComponent::localScaleX;
        ut["localScaleY"] = &TransformComponent::localScaleY;
        ut["localRotation"] = &TransformComponent::localRotation;

        ut["worldX"] = &TransformComponent::worldX;
        ut["worldY"] = &TransformComponent::worldY;
        ut["worldScaleX"] = &TransformComponent::worldScaleX;
        ut["worldScaleY"] = &TransformComponent::worldScaleY;
        ut["worldRotation"] = &TransformComponent::worldRotation;
    }

    void TransformComponent::SetFromLua(TransformComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.localX = t.get_or("localX", c.localX);
            c.localY = t.get_or("localY", c.localY);
            c.localScaleX = t.get_or("localScaleX", c.localScaleX);
            c.localScaleY = t.get_or("localScaleY", c.localScaleY);
            c.localRotation = t.get_or("localRotation", c.localRotation);
        }
    }

    REGISTER_COMPONENT(TransformComponent);
}

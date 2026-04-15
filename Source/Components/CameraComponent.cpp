#include "Components/CameraComponent.h"
#include "Core/Application.h"
#include "Core/ComponentRegistry.h"
#include "Core/Component.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    CameraComponent::CameraComponent()
        : zoom(1.0f), renderOrder(0), isVisible(true) {
        const auto& config = Application::GetInstance().GetConfig();
        viewport = {0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight};
    }

    void CameraComponent::LoadXml(CameraComponent& c, tinyxml2::XMLElement* el) {
        std::string target = el->Attribute("target") ? el->Attribute("target") : "";
        c.zoom = el->FloatAttribute("zoom", 1.0f);
    }

    void CameraComponent::Inspect(CameraComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Zoom: ");
        ImGui::DragFloat("##Zoom", &c.zoom, 0.01f, 0.1f, 10.0f);
        Label("Viewport: ");
        ImGui::DragFloat4("##Viewport", &c.viewport.x, 1.0f);
        Label("Render Order: ");
        ImGui::DragInt("##RenderOrder", &c.renderOrder);
        Label("Is Visible: ");
        ImGui::Checkbox("##IsVisible", &c.isVisible);
    }

    void CameraComponent::BindLua(sol::usertype<CameraComponent>& ut) {
        ut["viewport"] = &CameraComponent::viewport;
        ut["zoom"] = &CameraComponent::zoom;
        ut["renderOrder"] = &CameraComponent::renderOrder;
        ut["isVisible"] = &CameraComponent::isVisible;
    }

    void CameraComponent::SetFromLua(CameraComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.zoom = t.get_or("zoom", c.zoom);
            c.renderOrder = t.get_or("renderOrder", c.renderOrder);
            c.isVisible = t.get_or("isVisible", c.isVisible);
            // viewport?
        }
    }

    REGISTER_COMPONENT(CameraComponent);
}

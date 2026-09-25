#include "Components/RectangleComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    RectangleComponent::RectangleComponent(float width, float height, float cornerRadius)
        : width(width), height(height), cornerRadius(cornerRadius) {}

    void RectangleComponent::SaveXml(const RectangleComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("RectangleComponent")
            .SetAttribute("width", c.width)
            .SetAttribute("height", c.height)
            .SetAttribute("cornerRadius", c.cornerRadius);
        if (c.originX != 0.5f || c.originY != 0.5f) b.SetAttribute("originX", c.originX).SetAttribute("originY", c.originY);
    }

    void RectangleComponent::LoadXml(RectangleComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.width = el->FloatAttribute("width", 100.0f);
        c.height = el->FloatAttribute("height", 100.0f);
        c.cornerRadius = el->FloatAttribute("cornerRadius", 0.0f);
        c.originX = el->FloatAttribute("originX", 0.5f);
        c.originY = el->FloatAttribute("originY", 0.5f);
    }

    void RectangleComponent::Inspect(RectangleComponent& c, Entity e, ServiceLocator& services) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Width: ");
        ImGui::DragFloat("##Width", &c.width, 1.0f, 1.0f, 1000.0f);
        Label("Height: ");
        ImGui::DragFloat("##Height", &c.height, 1.0f, 1.0f, 1000.0f);
        Label("Corner Radius: ");
        ImGui::DragFloat("##CornerRadius", &c.cornerRadius, 0.01f, 0.0f, 1.0f);
        Label("Origin: ");
        ImGui::DragFloat2("##Origin", &c.originX, 0.01f, 0.0f, 1.0f);
    }

    void RectangleComponent::BindLua(sol::usertype<RectangleComponent>& ut) {
        ut["width"] = &RectangleComponent::width;
        ut["height"] = &RectangleComponent::height;
        ut["cornerRadius"] = &RectangleComponent::cornerRadius;
        ut["originX"] = &RectangleComponent::originX;
        ut["originY"] = &RectangleComponent::originY;
    }

    void RectangleComponent::SetFromLua(RectangleComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.width = t.get_or("width", c.width);
            c.height = t.get_or("height", c.height);
            c.cornerRadius = t.get_or("cornerRadius", c.cornerRadius);
        }
    }

    REGISTER_COMPONENT(RectangleComponent);
}

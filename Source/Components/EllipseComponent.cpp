#include "Components/EllipseComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    EllipseComponent::EllipseComponent(float radiusH, float radiusV) : radiusH(radiusH), radiusV(radiusV) {}

    void EllipseComponent::SaveXml(const EllipseComponent& c, XMLBuilder& builder) {
        builder.AddElement("EllipseComponent")
            .SetAttribute("radiusH", c.radiusH)
            .SetAttribute("radiusV", c.radiusV);
    }

    void EllipseComponent::LoadXml(EllipseComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.radiusH = el->FloatAttribute("radiusH", 50.0f);
        c.radiusV = el->FloatAttribute("radiusV", 50.0f);
    }

    void EllipseComponent::Inspect(EllipseComponent& c, Entity e, ServiceLocator& services) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Radius H: ");
        ImGui::DragFloat("##RadiusH", &c.radiusH, 1.0f, 1.0f, 1000.0f);
        Label("Radius V: ");
        ImGui::DragFloat("##RadiusV", &c.radiusV, 1.0f, 1.0f, 1000.0f);
    }

    void EllipseComponent::BindLua(sol::usertype<EllipseComponent>& ut) {
        ut["radiusH"] = &EllipseComponent::radiusH;
        ut["radiusV"] = &EllipseComponent::radiusV;
    }

    void EllipseComponent::SetFromLua(EllipseComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.radiusH = t.get_or("radiusH", c.radiusH);
            c.radiusV = t.get_or("radiusV", c.radiusV);
        }
    }

    REGISTER_COMPONENT(EllipseComponent);
}

#include "Components/LayerComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "imgui.h"
#include "tinyxml2.h"

namespace Elysium {
    LayerComponent::LayerComponent(const std::string& name, bool isVisible)
        : name(name), isVisible(isVisible) {}

    void LayerComponent::LoadXml(LayerComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        const char* name = el->Attribute("name");
        c.name = name ? name : "default";
        c.isVisible = el->BoolAttribute("visible", true);
    }

    void LayerComponent::SaveXml(const LayerComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("LayerComponent")
            .SetAttribute("name", c.name.c_str());
        if (!c.isVisible) b.SetAttribute("visible", false);
    }

    FieldList LayerComponent::Fields() {
        return {
            Field("Name", &LayerComponent::name, "name"),
            Field("Visible", &LayerComponent::isVisible, "visible"),
        };
    }

    void LayerComponent::BindLua(sol::usertype<LayerComponent>& ut) {
        ut["name"]      = &LayerComponent::name;
        ut["isVisible"] = &LayerComponent::isVisible;
    }

    void LayerComponent::SetFromLua(LayerComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.name = t.get_or("name", c.name);
        }
    }

    REGISTER_COMPONENT(LayerComponent);
}

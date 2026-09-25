#include "Components/NameComponent.h"
#include "Editor/Widgets.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    NameComponent::NameComponent(const std::string& name) : name(name) {}

    void NameComponent::LoadXml(NameComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        const char* val = el->Attribute("name");
        if(val) c.name = val;
    }

    void NameComponent::SaveXml(const NameComponent& c, XMLBuilder& builder) {
        builder.AddElement("NameComponent")
            .SetAttribute("name", c.name.c_str());
    }

    void NameComponent::Inspect(NameComponent& c, Entity e, ServiceLocator& services) {
        static char nameBuffer[256];
        strncpy(nameBuffer, c.name.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';

        PropertyLabel("Name");
        if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
            c.name = std::string(nameBuffer);
        }
    }

    void NameComponent::BindLua(sol::usertype<NameComponent>& ut) {
         ut["name"] = &NameComponent::name;
    }

    void NameComponent::SetFromLua(NameComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.name = t.get_or("name", c.name);
        } else if (v.is<std::string>()) {
            c.name = v.as<std::string>();
        }
    }
    
    REGISTER_COMPONENT(NameComponent);
}

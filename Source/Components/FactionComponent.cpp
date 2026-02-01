#include "Components/FactionComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void FactionComponent::LoadXml(FactionComponent& c, tinyxml2::XMLElement* el) {
        const char* val = el->Attribute("name");
        if(val) c.name = val;
    }

    void FactionComponent::Inspect(FactionComponent& c, Entity e) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Faction: ");
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1);

        static char nameBuffer[256];
        strncpy(nameBuffer, c.name.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';

        if (ImGui::InputText("##FactionName", nameBuffer, sizeof(nameBuffer))) {
            c.name = std::string(nameBuffer);
        }
    }

    void FactionComponent::BindLua(sol::usertype<FactionComponent>& ut) {
        ut["name"] = &FactionComponent::name;
    }

    void FactionComponent::SetFromLua(FactionComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.name = t.get_or("name", c.name);
        }
    }

    REGISTER_COMPONENT(FactionComponent);
}

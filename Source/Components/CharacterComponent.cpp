#include "Components/CharacterComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void CharacterComponent::LoadXml(CharacterComponent& c, tinyxml2::XMLElement* el) {
        c.id = el->IntAttribute("id", 0);
    }

    void CharacterComponent::Inspect(CharacterComponent& c, Entity e) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Char ID: ");
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1);
        ImGui::DragInt("##CharacterID", &c.id, 1.0f, 0);
    }

    void CharacterComponent::BindLua(sol::usertype<CharacterComponent>& ut) {
        ut["id"] = &CharacterComponent::id;
    }

    void CharacterComponent::SetFromLua(CharacterComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.id = t.get_or("id", c.id);
        }
    }

    REGISTER_COMPONENT(CharacterComponent);
}

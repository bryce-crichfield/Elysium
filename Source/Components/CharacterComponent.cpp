#include "Components/CharacterComponent.h"
#include "Core/Application.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "imgui.h"

namespace Elysium {

void CharacterComponent::LoadXml(CharacterComponent& c, tinyxml2::XMLElement* el) {
    const char* path = el->Attribute("character");
    if (path) {
        c.characterPath = path;
        auto& assetService = Application::GetInstance().GetService<Services::AssetService>();
        assetService.LoadAsset(AssetType::CHARACTER, Path(path));
    }
}

void CharacterComponent::SaveXml(const CharacterComponent& c, XMLBuilder& builder) {
    if (c.characterPath.empty()) return;
    builder.AddElement("CharacterComponent")
        .SetAttribute("character", c.characterPath.c_str());
}

void CharacterComponent::Inspect(CharacterComponent& c, Entity /*e*/) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Character: ");
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(-1);

    static char buf[256];
    strncpy(buf, c.characterPath.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    if (ImGui::InputText("##CharacterPath", buf, sizeof(buf))) {
        c.characterPath = buf;
    }
}

void CharacterComponent::BindLua(sol::usertype<CharacterComponent>& ut) {
    ut["characterPath"] = &CharacterComponent::characterPath;
}

void CharacterComponent::SetFromLua(CharacterComponent& c, sol::object v) {
    if (v.is<sol::table>()) {
        sol::table t = v.as<sol::table>();
        c.characterPath = t.get_or("characterPath", c.characterPath);
    } else if (v.is<std::string>()) {
        c.characterPath = v.as<std::string>();
    }
}

REGISTER_COMPONENT(CharacterComponent);

}  // namespace Elysium

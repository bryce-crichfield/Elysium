#include "Components/SelectionComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void SelectionComponent::LoadXml(SelectionComponent& c, tinyxml2::XMLElement* el) {
    }

    void SelectionComponent::Inspect(SelectionComponent& c, Entity e) {
        ImGui::Text("Selection component (Tag)");
    }

    void SelectionComponent::BindLua(sol::usertype<SelectionComponent>& ut) {
    }

    void SelectionComponent::SetFromLua(SelectionComponent& c, sol::object v) {
    }

    REGISTER_COMPONENT(SelectionComponent);
}

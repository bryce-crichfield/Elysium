#include "Components/TileComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void TileComponent::LoadXml(TileComponent& c, tinyxml2::XMLElement* el) {
        // No properties
    }

    void TileComponent::Inspect(TileComponent& c, Entity e) {
        ImGui::Text("Tile component (no properties)");
    }

    void TileComponent::BindLua(sol::usertype<TileComponent>& ut) {
        // No properties
    }

    void TileComponent::SetFromLua(TileComponent& c, sol::object v) {
        // No properties
    }

    REGISTER_COMPONENT(TileComponent);
}

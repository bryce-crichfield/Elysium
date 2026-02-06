#include "Components/DirectionComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void DirectionComponent::LoadXml(DirectionComponent& c, tinyxml2::XMLElement* el) {
        // No attributes to load for now
    }

    void DirectionComponent::Inspect(DirectionComponent& c, Entity e) {
    }

    void DirectionComponent::BindLua(sol::usertype<DirectionComponent>& ut) {
    }

    void DirectionComponent::SetFromLua(DirectionComponent& c, sol::object v) {
    }

    REGISTER_COMPONENT(DirectionComponent);
}

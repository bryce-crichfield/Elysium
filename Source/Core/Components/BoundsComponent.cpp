#include "Core/Components/BoundsComponent.h"
#include "Core/ComponentRegistry.h"

namespace Elysium {
    void BoundsComponent::LoadXml(BoundsComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        // Computed component
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        return Colors::White;
    }

    void BoundsComponent::BindLua(sol::usertype<BoundsComponent>& ut) {
        ut["bounds"] = &BoundsComponent::bounds;
        ut["isDragging"] = &BoundsComponent::isDragging;
        ut["debugColor"] = sol::property(
            [](BoundsComponent& b) { return b.debugColor; },
            [](BoundsComponent& b, sol::object v) { b.debugColor = ObjectToColor(v); });
    }

    void BoundsComponent::SetFromLua(BoundsComponent& c, sol::object v) {
        // Read-only usually? Or limited set.
    }

    REGISTER_COMPONENT(BoundsComponent);
}

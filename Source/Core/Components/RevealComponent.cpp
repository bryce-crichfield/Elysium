#include "Core/Components/RevealComponent.h"
#include "Core/ComponentRegistry.h"
#include "tinyxml2.h"

namespace Elysium {

    void RevealComponent::LoadXml(RevealComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.height = el->FloatAttribute("height", c.height);
    }

    void RevealComponent::SaveXml(const RevealComponent& c, XMLBuilder& builder) {
        builder.AddElement("RevealComponent")
            .SetAttribute("height", c.height);
    }

    FieldList RevealComponent::Fields() {
        return {
            Field("Height", &RevealComponent::height, "height").Speed(1.0f),
        };
    }

    REGISTER_COMPONENT(RevealComponent);
}

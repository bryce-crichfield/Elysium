#include "Core/Components/LightComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"

namespace Elysium {

    void LightComponent::LoadXml(LightComponent& c, tinyxml2::XMLElement* el, ServiceLocator&) {
        if (const char* color = el->Attribute("color")) c.color = ParseHexColor(color, c.color);
        c.intensity = el->FloatAttribute("intensity", c.intensity);
        c.radius    = el->FloatAttribute("radius", c.radius);
        c.height    = el->FloatAttribute("height", c.height);
        c.flicker   = el->FloatAttribute("flicker", c.flicker);
        c.vision    = el->BoolAttribute("vision", c.vision);
    }

    void LightComponent::SaveXml(const LightComponent& c, XMLBuilder& builder) {
        builder.AddElement("LightComponent")
            .SetAttribute("color", ColorToHex(c.color).c_str())
            .SetAttribute("intensity", c.intensity)
            .SetAttribute("radius", c.radius)
            .SetAttribute("height", c.height)
            .SetAttribute("flicker", c.flicker)
            .SetAttribute("vision", c.vision);
    }

    FieldList LightComponent::Fields() {
        return {
            Field("Color", &LightComponent::color, "color"),
            Field("Intensity", &LightComponent::intensity, "intensity").Speed(0.05f).Range(0.0f, 50.0f),
            Field("Radius", &LightComponent::radius, "radius").Range(1.0f, 4000.0f),
            Field("Height", &LightComponent::height, "height").Range(0.0f, 1000.0f),
            Field("Flicker", &LightComponent::flicker, "flicker").Speed(0.01f).Range(0.0f, 1.0f),
            Field("Vision", &LightComponent::vision, "vision"),
        };
    }

    REGISTER_COMPONENT(LightComponent);
}

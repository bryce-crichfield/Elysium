#pragma once
#include "Core/Component.h"

namespace Elysium {
    // Keeps its entity in view: the models standing between it and the camera fade to a dither.
    // The engine's only source of that fade; it has nothing to do with lighting or fog of war.
    // The ray leaves `height` world units above the entity's position.
    struct RevealComponent {
        float height = 32.0f;

        static constexpr const char* Name() { return "Reveal"; }
        static constexpr const char* XmlTag() { return "RevealComponent"; }

        static void LoadXml(RevealComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const RevealComponent& c, XMLBuilder& builder);
        static FieldList Fields();
    };
}

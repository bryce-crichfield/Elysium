#pragma once
#include "Core/Component.h"
#include "raylib.h"

namespace Elysium {
    struct EllipseComponent {
        float radiusH, radiusV;
        Color background;
        Color border;

        EllipseComponent(float radiusH = 50.0f, float radiusV = 50.0f, Color background = {}, Color border = {});

        static constexpr const char* Name() { return "Ellipse"; }
        static constexpr const char* XmlTag() { return "EllipseComponent"; }

        static void LoadXml(EllipseComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const EllipseComponent& c, XMLBuilder& builder);
        static void Inspect(EllipseComponent& c, Entity e);
        static void BindLua(sol::usertype<EllipseComponent>& ut);
        static void SetFromLua(EllipseComponent& c, sol::object v);
    };
}

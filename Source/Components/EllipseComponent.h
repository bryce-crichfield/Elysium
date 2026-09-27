#pragma once
#include "Core/Component.h"
#include "Core/Graphics.h"

namespace Elysium {
    struct EllipseComponent {
        float radiusH, radiusV;

        EllipseComponent(float radiusH = 50.0f, float radiusV = 50.0f);

        static constexpr const char* Name() { return "Ellipse"; }
        static constexpr InspectorOrder Order = InspectorOrder::Geometry;
        static constexpr const char* XmlTag() { return "EllipseComponent"; }

        static void LoadXml(EllipseComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const EllipseComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<EllipseComponent>& ut);
        static void SetFromLua(EllipseComponent& c, sol::object v);
    };
}

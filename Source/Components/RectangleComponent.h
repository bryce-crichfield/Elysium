#pragma once
#include "Core/Component.h"
#include "Core/Graphics.h"
#include "Core/MathTypes.h"
#include <string>

namespace Elysium {
    struct RectangleComponent {
        float width, height;
        float cornerRadius;  // raylib roundness ratio; 0 = square corners
        // World-space pivot as a fraction of the box: 0.5,0.5 centers the box on the
        // entity's position; a sprite's feet sit lower (e.g. 0.5,0.69). Screen-space
        // rectangles are positioned by their top-left and ignore it.
        float originX = 0.5f;
        float originY = 0.5f;

        RectangleComponent(float width = 1, float height = 1, float cornerRadius = 0.0f);

        static constexpr const char* Name() { return "Rectangle"; }
        static constexpr InspectorOrder Order = InspectorOrder::Geometry;
        static constexpr const char* XmlTag() { return "RectangleComponent"; }

        static void LoadXml(RectangleComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const RectangleComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<RectangleComponent>& ut);
        static void SetFromLua(RectangleComponent& c, sol::object v);
    };
}

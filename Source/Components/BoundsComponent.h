#pragma once
#include "Core/Component.h"
#include "Core/Event.h"
#include "Components/LayerComponent.h"
#include "Core/Scene.h"
#include "Core/Graphics.h"
#include "Core/MathTypes.h"

namespace Elysium {
    struct BoundsComponent {
        Rectangle bounds;  // Bounding box in the entity's layer coordinate space
        SceneLayerSpace space = SceneLayerSpace::World3D;  // Coordinate space of bounds
        bool isDragging;   // Is this entity currently being dragged
        Color debugColor;  // Color to draw debug bounds

        BoundsComponent()
            : bounds({0, 0, 0, 0}), isDragging(false), debugColor(Colors::Red) {}

        BoundsComponent(Rectangle rect, Color color)
            : bounds(rect), isDragging(false), debugColor(color) {}


        static constexpr const char* Name() { return "Bounds"; }
        static constexpr InspectorOrder Order = InspectorOrder::Geometry;
        static constexpr const char* XmlTag() { return "BoundsComponent"; }

        static void LoadXml(BoundsComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void Inspect(BoundsComponent& c, Entity e, ServiceLocator& services);
        static void BindLua(sol::usertype<BoundsComponent>& ut);
        static void SetFromLua(BoundsComponent& c, sol::object v);
    };
}

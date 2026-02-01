#pragma once
#include "Core/Component.h"
#include "Core/Event.h"
#include "Core/Signal.h"
#include "raylib.h"

namespace Elysium {
    struct BoundsComponent {
        Rectangle bounds;  // Bounding box in world space
        bool isDragging;   // Is this entity currently being dragged
        Color debugColor;  // Color to draw debug bounds

        BoundsComponent()
            : bounds({0, 0, 0, 0}), isDragging(false), debugColor(RED) {}

        BoundsComponent(Rectangle rect, Color color)
            : bounds(rect), isDragging(false), debugColor(color) {}

        Signal<Event> OnPickEvent;

        static constexpr const char* Name() { return "Bounds"; }
        static constexpr const char* XmlTag() { return "BoundsComponent"; }

        static void LoadXml(BoundsComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(BoundsComponent& c, Entity e);
        static void BindLua(sol::usertype<BoundsComponent>& ut);
        static void SetFromLua(BoundsComponent& c, sol::object v);
    };
}

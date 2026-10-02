#pragma once
#include "Core/Component.h"
#include <cstdint>

namespace Elysium {
    // Local fields are the authored, parent-relative transform and are the only
    // source of truth persisted to XML. World fields are a cache recomputed every
    // frame by TransformSystem by composing local -> world down the entity
    // hierarchy; nothing else should assign to them.
    struct TransformComponent {
        float localX = 0.0f, localY = 0.0f;
        // Height above the ground, in world units (3D: see Core/World3D.h). x and y stay the
        // ground position, so everything that walks the ground keeps working in 2D.
        float localZ = 0.0f;
        float localScaleX = 1.0f, localScaleY = 1.0f;
        float localRotation = 0.0f;

        float worldX = 0.0f, worldY = 0.0f;
        float worldZ = 0.0f;
        float worldScaleX = 1.0f, worldScaleY = 1.0f;
        float worldRotation = 0.0f;
        uint32_t worldDepth = 0;  // hierarchy depth (0 = root), cached by TransformSystem

        TransformComponent(float x = 0.0f, float y = 0.0f);

        static constexpr const char* Name() { return "Transform"; }
        static constexpr bool PlacementOwned = true;
        static constexpr InspectorOrder Order = InspectorOrder::Transform;
        static constexpr const char* XmlTag() { return "TransformComponent"; }

        static void LoadXml(TransformComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const TransformComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<TransformComponent>& ut);
        static void SetFromLua(TransformComponent& c, sol::object v);
    };
}

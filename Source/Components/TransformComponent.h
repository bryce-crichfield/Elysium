#pragma once
#include "Core/Component.h"

namespace Elysium {
    // Local fields are the authored, parent-relative transform and are the only
    // source of truth persisted to XML. World fields are a cache recomputed every
    // frame by TransformSystem by composing local -> world down the entity
    // hierarchy; nothing else should assign to them.
    struct TransformComponent {
        float localX = 0.0f, localY = 0.0f;
        float localScaleX = 1.0f, localScaleY = 1.0f;
        float localRotation = 0.0f;

        float worldX = 0.0f, worldY = 0.0f;
        float worldScaleX = 1.0f, worldScaleY = 1.0f;
        float worldRotation = 0.0f;

        TransformComponent(float x = 0.0f, float y = 0.0f);

        static constexpr const char* Name() { return "Transform"; }
        static constexpr const char* XmlTag() { return "TransformComponent"; }

        static void LoadXml(TransformComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const TransformComponent& c, XMLBuilder& builder);
        static void Inspect(TransformComponent& c, Entity e);
        static void BindLua(sol::usertype<TransformComponent>& ut);
        static void SetFromLua(TransformComponent& c, sol::object v);
    };
}

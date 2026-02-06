#pragma once
#include "Core/Component.h"
#include "raylib.h"

namespace Elysium {
    struct ColliderComponent {
        float width = 32.0f;
        float height = 32.0f;
        float offsetX = 0.0f;  // Offset from entity position
        float offsetY = 0.0f;
        bool isTrigger = false;  // Triggers don't cause physical response, just detection

        ColliderComponent() = default;
        ColliderComponent(float w, float h, float ox = 0.0f, float oy = 0.0f)
            : width(w), height(h), offsetX(ox), offsetY(oy), isTrigger(false) {}

        // Get the world-space rectangle given entity position
        Rectangle GetRect(float posX, float posY) const {
            return Rectangle{
                posX + offsetX - width * 0.5f,
                posY + offsetY - height * 0.5f,
                width,
                height
            };
        }

        static constexpr const char* Name() { return "Collider"; }
        static constexpr const char* XmlTag() { return "ColliderComponent"; }

        static void LoadXml(ColliderComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(ColliderComponent& c, Entity e);
        static void BindLua(sol::usertype<ColliderComponent>& ut);
        static void SetFromLua(ColliderComponent& c, sol::object v);
    };
}

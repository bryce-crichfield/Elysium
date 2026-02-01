#pragma once
#include "Core/Component.h"
#include "raylib.h"

namespace Elysium {
    struct KinematicsComponent {
        Vector2 velocity;
        Vector2 acceleration;
        float friction; // 0.0 = no friction, 1.0 = instant stop per second
        float maxSpeed;

        KinematicsComponent(float maxSpd = 200.0f, float f = 5.0f) 
            : velocity({0,0}), acceleration({0,0}), friction(f), maxSpeed(maxSpd) {}

        static constexpr const char* Name() { return "Kinematics"; }
        static constexpr const char* XmlTag() { return "KinematicsComponent"; }

        static void LoadXml(KinematicsComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(KinematicsComponent& c, Entity e);
        static void BindLua(sol::usertype<KinematicsComponent>& ut);
        static void SetFromLua(KinematicsComponent& c, sol::object v);
    };
}

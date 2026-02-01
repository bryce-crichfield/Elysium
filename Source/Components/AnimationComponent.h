#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    struct AnimationComponent {
        std::string marker;
        int currentFrame = 0;
        int start, end;
        float frameDuration, elapsed = 0;
        bool loop = false;

        AnimationComponent() = default;
        AnimationComponent(std::string marker);

        static constexpr const char* Name() { return "Animation"; }
        static constexpr const char* XmlTag() { return "AnimationComponent"; }

        static void LoadXml(AnimationComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(AnimationComponent& c, Entity e);
        static void BindLua(sol::usertype<AnimationComponent>& ut);
        static void SetFromLua(AnimationComponent& c, sol::object v);
    };
}

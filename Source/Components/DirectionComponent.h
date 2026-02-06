#pragma once
#include "Core/Component.h"

namespace Elysium {
    enum class Direction {
        NONE = 0,
        UP,
        DOWN,
        LEFT,
        RIGHT
    };

    struct DirectionComponent {
        Direction currentDirection = Direction::NONE;
        Direction previousDirection = Direction::NONE;
        bool hasChanged = false;

        DirectionComponent() = default;
        DirectionComponent(Direction dir) : currentDirection(dir), previousDirection(dir) {}

        void SetDirection(Direction newDir);
        void ClearChanged() { hasChanged = false; }

        static constexpr const char* Name() { return "Direction"; }
        static constexpr const char* XmlTag() { return "DirectionComponent"; }

        static void LoadXml(DirectionComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(DirectionComponent& c, Entity e);
        static void BindLua(sol::usertype<DirectionComponent>& ut);
        static void SetFromLua(DirectionComponent& c, sol::object v);
    };
}

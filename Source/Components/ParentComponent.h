#pragma once
#include "Core/Component.h"
#include "Core/Entity.h"
#include <string>

namespace Elysium {
    // Placed on a child entity. targetName is the parent's NameComponent name,
    // used for XML authoring and round-trip save/load. parent is the resolved
    // Entity ID populated by the scene loader's hierarchy resolution pass.
    struct ParentComponent {
        Entity parent = INVALID_ENTITY;
        std::string targetName;

        ParentComponent() = default;
        ParentComponent(Entity parent, const std::string& targetName) : parent(parent), targetName(targetName) {}

        static constexpr const char* Name() { return "Parent"; }
        static constexpr const char* XmlTag() { return "ParentComponent"; }

        static void LoadXml(ParentComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const ParentComponent& c, XMLBuilder& builder);
        static void Inspect(ParentComponent& c, Entity e);
        static void BindLua(sol::usertype<ParentComponent>& ut);
    };
}

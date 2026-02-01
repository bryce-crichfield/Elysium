#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct TileComponent {
        // ACTS LIKE A TYPE FLAG
        TileComponent() = default;

        static constexpr const char* Name() { return "Tile"; }
        static constexpr const char* XmlTag() { return "TileComponent"; }

        static void LoadXml(TileComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(TileComponent& c, Entity e);
        static void BindLua(sol::usertype<TileComponent>& ut);
        static void SetFromLua(TileComponent& c, sol::object v);
    };
}

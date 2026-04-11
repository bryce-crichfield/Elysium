#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct TileComponent {
        float tileWidth = 32.0f;
        float tileHeight = 32.0f;
        bool isIsometric = false;

        TileComponent() = default;
        TileComponent(float width, float height, bool isometric = false)
            : tileWidth(width), tileHeight(height), isIsometric(isometric) {}

        static constexpr const char* Name() { return "Tile"; }
        static constexpr const char* XmlTag() { return "TileComponent"; }

        static void LoadXml(TileComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const TileComponent& c, XMLBuilder& builder);
        static void Inspect(TileComponent& c, Entity e);
        static void BindLua(sol::usertype<TileComponent>& ut);
        static void SetFromLua(TileComponent& c, sol::object v);
    };
}

#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <string>

namespace Elysium {
    struct TileComponent {
        std::string tileName;                   // path to tile asset, e.g. "Tiles/Tile/Tile.xml"
        std::string variantName = "default";    // which variant to render
        bool isIsometric = false;               // used by RenderSystem to detect scene projection
        Color tint = WHITE;                     // per-instance draw tint

        // Set by SceneLoader from Tilemap attributes — grid cell size in world units.
        // Not part of the Tile asset; used by SceneSaver to reverse-engineer grid coords.
        float tileWidth  = 64.0f;
        float tileHeight = 32.0f;

        TileComponent() = default;
        TileComponent(const std::string& tile, const std::string& variant, bool isometric,
                      float tw, float th)
            : tileName(tile), variantName(variant), isIsometric(isometric),
              tileWidth(tw), tileHeight(th) {}

        static constexpr const char* Name() { return "Tile"; }
        static constexpr const char* XmlTag() { return "TileComponent"; }

        static void LoadXml(TileComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const TileComponent& c, XMLBuilder& builder);
        static void Inspect(TileComponent& c, Entity e);
        static void BindLua(sol::usertype<TileComponent>& ut);
        static void SetFromLua(TileComponent& c, sol::object v);
    };
}

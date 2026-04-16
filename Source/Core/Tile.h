#pragma once

#include <string>
#include <unordered_map>

namespace Elysium {

struct TileVariant {
    std::string name;
    size_t row = 0;
    size_t col = 0;
};

struct TileSheet {
    std::string path;   // relative to "Tiles/", e.g. "Tile/Tile.png"
    size_t rows = 1;
    size_t cols = 1;
};

struct Tile {
    std::string name;
    float originX = 0.5f;   // normalized: 0 = left,  1 = right
    float originY = 0.5f;   // normalized: 0 = top,   1 = bottom
    TileSheet sheet;
    std::unordered_map<std::string, TileVariant> variants;

    bool IsEmpty() const { return name.empty(); }

    static Tile LoadFromXml(const std::string& filepath);
};

}  // namespace Elysium

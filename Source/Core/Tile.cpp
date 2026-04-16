#include "Tile.h"
#include <tinyxml2.h>
#include <stdexcept>

namespace Elysium {

Tile Tile::LoadFromXml(const std::string& filepath) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filepath.c_str()) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to load tile XML: " + filepath);
    }

    auto* tileElem = doc.FirstChildElement("Tile");
    if (!tileElem) {
        throw std::runtime_error("Missing <Tile> element in: " + filepath);
    }

    Tile tile;
    tile.name    = tileElem->Attribute("name") ? tileElem->Attribute("name") : "";
    tile.originX = tileElem->FloatAttribute("originX", 0.5f);
    tile.originY = tileElem->FloatAttribute("originY", 0.5f);

    // Parse sheet
    auto* sheetElem = tileElem->FirstChildElement("Sheet");
    if (sheetElem) {
        tile.sheet.path = sheetElem->Attribute("path") ? sheetElem->Attribute("path") : "";
        tile.sheet.rows = sheetElem->UnsignedAttribute("rows", 1);
        tile.sheet.cols = sheetElem->UnsignedAttribute("cols", 1);
    }

    // Parse variants — each is a single cell: index="row:col"
    for (auto* varElem = tileElem->FirstChildElement("Variant");
         varElem;
         varElem = varElem->NextSiblingElement("Variant")) {
        const char* name  = varElem->Attribute("name");
        const char* index = varElem->Attribute("index");
        if (!name || !index) continue;

        std::string indexStr(index);
        size_t colonPos = indexStr.find(':');
        if (colonPos == std::string::npos) continue;

        TileVariant variant;
        variant.name = name;
        variant.row  = std::stoul(indexStr.substr(0, colonPos));
        variant.col  = std::stoul(indexStr.substr(colonPos + 1));
        tile.variants[name] = std::move(variant);
    }

    return tile;
}

}  // namespace Elysium

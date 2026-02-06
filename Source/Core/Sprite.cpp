#include "Sprite.h"
#include <tinyxml2.h>
#include <sstream>
#include <stdexcept>

namespace Elysium {

static std::vector<size_t> ParseIndices(const std::string& indicies, size_t rows, size_t cols) {
    std::vector<size_t> result;

    // Find the colon separator
    size_t colonPos = indicies.find(':');
    if (colonPos == std::string::npos) {
        throw std::runtime_error("Invalid indices format: missing ':'");
    }

    // Parse row number
    size_t row = std::stoul(indicies.substr(0, colonPos));
    std::string colPart = indicies.substr(colonPos + 1);

    if (colPart == "*") {
        // "x:*" - all columns in row x
        for (size_t col = 0; col < cols; ++col) {
            result.push_back(row * cols + col);
        }
    } else if (colPart.find(',') != std::string::npos) {
        // "x:y0,y1,..." - specific columns
        std::stringstream ss(colPart);
        std::string token;
        while (std::getline(ss, token, ',')) {
            size_t col = std::stoul(token);
            result.push_back(row * cols + col);
        }
    } else {
        // "x:y" - single cell
        size_t col = std::stoul(colPart);
        result.push_back(row * cols + col);
    }

    return result;
}

Sprite Sprite::LoadFromXml(const std::string& filepath) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filepath.c_str()) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to load sprite XML: " + filepath);
    }

    auto* spriteElem = doc.FirstChildElement("Sprite");
    if (!spriteElem) {
        throw std::runtime_error("Missing <Sprite> element in: " + filepath);
    }

    Sprite sprite;
    sprite.name = spriteElem->Attribute("name") ? spriteElem->Attribute("name") : "";

    // Parse shared sequences (store name -> indices string pairs)
    std::vector<std::pair<std::string, std::string>> sharedSequences;
    auto* sequencesElem = spriteElem->FirstChildElement("Sequences");
    if (sequencesElem) {
        for (auto* seqElem = sequencesElem->FirstChildElement("Sequence");
             seqElem;
             seqElem = seqElem->NextSiblingElement("Sequence")) {
            const char* name = seqElem->Attribute("name");
            const char* indicies = seqElem->Attribute("indicies");
            if (name && indicies) {
                sharedSequences.emplace_back(name, indicies);
            }
        }
    }

    // Parse sheets
    for (auto* sheetElem = spriteElem->FirstChildElement("Sheet");
         sheetElem;
         sheetElem = sheetElem->NextSiblingElement("Sheet")) {
        SpriteSheet sheet;
        sheet.name = sheetElem->Attribute("name") ? sheetElem->Attribute("name") : "";
        sheet.path = sheetElem->Attribute("path") ? sheetElem->Attribute("path") : "";
        sheet.rows = sheetElem->UnsignedAttribute("rows", 1);
        sheet.cols = sheetElem->UnsignedAttribute("columns", 1);
        sheet.width = 0;  // To be set when texture is loaded
        sheet.height = 0;

        // Resolve shared sequences for this sheet
        for (const auto& [seqName, indiciesStr] : sharedSequences) {
            SpriteSequence seq;
            seq.name = seqName;
            seq.indices = ParseIndices(indiciesStr, sheet.rows, sheet.cols);
            sheet.sequences[seqName] = std::move(seq);
        }

        sprite.sheets[sheet.name] = std::move(sheet);
    }

    return sprite;
}

}  // namespace Elysium

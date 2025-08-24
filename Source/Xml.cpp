#include "Xml.h"
#include "Services/LogService.h"
#include <stdexcept>
#include <tinyxml2.h>

namespace Elysium
{
Color ParseHexColor(const std::string& hex, Color defaultColor) {
    if (hex.empty() || hex[0] != '#') return defaultColor;

    std::string colorStr = hex.substr(1); // Remove '#'

    if (colorStr.length() == 6) {
        // #RRGGBB format
        unsigned int colorValue = std::stoul(colorStr, nullptr, 16);
        return {
            (unsigned char)((colorValue >> 16) & 0xFF), // R
            (unsigned char)((colorValue >> 8) & 0xFF),  // G
            (unsigned char)(colorValue & 0xFF),         // B
            255                                         // A (full opacity)
        };
    } else if (colorStr.length() == 8) {
        // #RRGGBBAA format
        unsigned int colorValue = std::stoul(colorStr, nullptr, 16);
        return {
            (unsigned char)((colorValue >> 24) & 0xFF), // R
            (unsigned char)((colorValue >> 16) & 0xFF), // G
            (unsigned char)((colorValue >> 8) & 0xFF),  // B
            (unsigned char)(colorValue & 0xFF)          // A
        };
    }

    return defaultColor;
}

// Processes XML '<Include src="path" />' tags by loading and merging referenced files into main document
void ProcessIncludes(tinyxml2::XMLDocument& doc, const std::string& basePath) {
    for (tinyxml2::XMLElement* includeElem = doc.RootElement()->FirstChildElement("Include");
         includeElem != nullptr;
         includeElem = includeElem->NextSiblingElement("Include"))
    {
        const char* src = includeElem->Attribute("src");
        if (!src) continue;

        // Load the included file
        tinyxml2::XMLDocument includeDoc;
        if (includeDoc.LoadFile((basePath + src).c_str()) == tinyxml2::XML_SUCCESS) {
            // Grab the root node of the included file
            tinyxml2::XMLElement* includedRoot = includeDoc.RootElement();
            if (includedRoot) {
                // Deep clone into main document
                tinyxml2::XMLElement* clone = includedRoot->DeepClone(&doc)->ToElement();
                includeElem->Parent()->InsertAfterChild(includeElem, clone);
            }
        } else {
            // LOG_ERROR("Scene", "Failed to load include");
        }

        // Remove the <include> tag
        includeElem->Parent()->DeleteChild(includeElem);
        // Restart search from the beginning after modification
        includeElem = doc.RootElement()->FirstChildElement("Include");
    }
}

} // namespace Elysium::Xml

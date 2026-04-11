#include "Core/Xml.h"
#include <tinyxml2.h>
#include <stdexcept>
#include <cstdio>
#include "Services/LogService.h"

namespace Elysium {

std::string ColorToHex(Color color) {
    if (color.a == 0)
        return "";
    char hex[10];  // '#' + 8 hex digits + null terminator
    snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return std::string(hex);
}
Color ParseHexColor(const std::string& hex, Color defaultColor) {
    if (hex.empty() || hex[0] != '#')
        return defaultColor;

    std::string colorStr = hex.substr(1);  // Remove '#'

    if (colorStr.length() == 6) {
        // #RRGGBB format
        unsigned int colorValue = std::stoul(colorStr, nullptr, 16);
        return {
            (unsigned char)((colorValue >> 16) & 0xFF),  // R
            (unsigned char)((colorValue >> 8) & 0xFF),   // G
            (unsigned char)(colorValue & 0xFF),          // B
            255                                          // A (full opacity)
        };
    } else if (colorStr.length() == 8) {
        // #RRGGBBAA format
        unsigned int colorValue = std::stoul(colorStr, nullptr, 16);
        return {
            (unsigned char)((colorValue >> 24) & 0xFF),  // R
            (unsigned char)((colorValue >> 16) & 0xFF),  // G
            (unsigned char)((colorValue >> 8) & 0xFF),   // B
            (unsigned char)(colorValue & 0xFF)           // A
        };
    }

    return defaultColor;
}

// Processes XML '<Include src="path" />' tags by loading and merging referenced files into main document
bool ProcessIncludes(tinyxml2::XMLDocument& doc, const std::string& basePath) {
    for (tinyxml2::XMLElement* includeElem = doc.RootElement()->FirstChildElement("Include");
         includeElem != nullptr;
         includeElem = includeElem->NextSiblingElement("Include")) {
        const char* src = includeElem->Attribute("src");
        if (!src)
            continue;

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
            LOG_ERRORF("Scene", "Failed to load include file: %s", (basePath + src).c_str());
        }

        // Remove the <include> tag
        includeElem->Parent()->DeleteChild(includeElem);
        // Restart search from the beginning after modification
        includeElem = doc.RootElement()->FirstChildElement("Include");
    }

    return true;
}

bool LoadXml(const std::string& filePath, tinyxml2::XMLDocument& doc) {
    tinyxml2::XMLError result = doc.LoadFile(filePath.c_str());
    if (result != tinyxml2::XML_SUCCESS) {
        LOG_ERRORF("Xml", "Failed to load xml file: %s", filePath.c_str());
        return false;
    } else {
        LOG_INFO("Xml", "XML file loaded successfully");
    }

    if (!ProcessIncludes(doc, "")) {
        LOG_ERROR("Xml", "Failed to process includes.");
        return false;
    }

    return true;
}

bool SaveXml(const std::string& filePath, tinyxml2::XMLDocument& doc) {
    tinyxml2::XMLError result = doc.SaveFile(filePath.c_str());
    if (result != tinyxml2::XML_SUCCESS) {
        LOG_ERRORF("Xml", "Failed to save xml file: %s", filePath.c_str());
        return false;
    } else {
        LOG_INFO("Xml", "XML file saved successfully");
    }
    return true;
}

}  // namespace Elysium

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

std::string GetBasePath(const std::string& filePath) {
    auto sep = filePath.find_last_of("/\\");
    if (sep != std::string::npos) {
        return filePath.substr(0, sep + 1);
    }
    return "";
}

bool LoadXml(const std::string& filePath, tinyxml2::XMLDocument& doc) {
    tinyxml2::XMLError result = doc.LoadFile(filePath.c_str());
    if (result != tinyxml2::XML_SUCCESS) {
        LOG_ERRORF("Xml", "Failed to load xml file: %s", filePath.c_str());
        return false;
    } else {
        LOG_INFO("Xml", "XML file loaded successfully");
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

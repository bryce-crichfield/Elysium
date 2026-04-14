#include "Core/Xml.h"
#include <tinyxml2.h>
#include <stdexcept>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>
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

// Processes XML '<Include src="path" key="value" .../>' tags.
// All attributes besides "src" are treated as template parameters: every occurrence
// of "$key" in the included file's raw text is replaced with "value" before parsing.
// This lets prefab files use placeholders like $name, $x, $y.
bool ProcessIncludes(tinyxml2::XMLDocument& doc, const std::string& basePath) {
    while (true) {
        tinyxml2::XMLElement* includeElem = doc.RootElement()->FirstChildElement("Include");
        if (!includeElem) break;

        const char* src = includeElem->Attribute("src");
        if (!src) {
            includeElem->Parent()->DeleteChild(includeElem);
            continue;
        }

        // Collect template parameters (all attributes except "src")
        std::unordered_map<std::string, std::string> params;
        for (const tinyxml2::XMLAttribute* attr = includeElem->FirstAttribute();
             attr != nullptr; attr = attr->Next()) {
            if (std::string(attr->Name()) != "src") {
                params[attr->Name()] = attr->Value();
            }
        }

        // Read the included file as raw text
        std::string fullPath = basePath + src;
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            LOG_ERRORF("Scene", "Failed to open include file: %s", fullPath.c_str());
            includeElem->Parent()->DeleteChild(includeElem);
            continue;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();

        // Substitute $key → value for each template parameter
        for (const auto& [key, value] : params) {
            std::string token = "$" + key;
            size_t pos = 0;
            while ((pos = content.find(token, pos)) != std::string::npos) {
                content.replace(pos, token.length(), value);
                pos += value.length();
            }
        }

        // Parse the substituted text and inject the root element
        tinyxml2::XMLDocument includeDoc;
        if (includeDoc.Parse(content.c_str()) == tinyxml2::XML_SUCCESS) {
            tinyxml2::XMLElement* includedRoot = includeDoc.RootElement();
            if (includedRoot) {
                tinyxml2::XMLElement* clone = includedRoot->DeepClone(&doc)->ToElement();
                includeElem->Parent()->InsertAfterChild(includeElem, clone);
            }
        } else {
            LOG_ERRORF("Scene", "Failed to parse include file: %s", fullPath.c_str());
        }

        includeElem->Parent()->DeleteChild(includeElem);
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

    // Derive basePath from the file's directory so <Include src="..."> can use paths
    // relative to the scene file rather than the working directory.
    std::string basePath;
    auto sep = filePath.find_last_of("/\\");
    if (sep != std::string::npos) {
        basePath = filePath.substr(0, sep + 1);
    }

    if (!ProcessIncludes(doc, basePath)) {
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

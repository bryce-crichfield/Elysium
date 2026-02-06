#include "Sprite.h"
#include <tinyxml2.h>
#include <stdexcept>

namespace Elysium {
Sprite Sprite::LoadFromXml(const std::string& filepath) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filepath.c_str()) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to load XML file");
    }

    auto root = doc.FirstChildElement("Sprite");
    if (!root)
        throw std::runtime_error("Missing Sprite root element");

    Sprite sprite;
    sprite.name = root->Attribute("name") ? root->Attribute("name") : "";

    // Load sheets
    for (auto sheetEl = root->FirstChildElement("Sheet"); sheetEl; sheetEl = sheetEl->NextSiblingElement("Sheet")) {
        SpriteSheet sheet;
        std::string sheetName = sheetEl->Attribute("name") ? sheetEl->Attribute("name") : "";
        sheet.file = sheetEl->Attribute("file") ? sheetEl->Attribute("file") : "";
        sheet.width = sheetEl->UnsignedAttribute("width");
        sheet.height = sheetEl->UnsignedAttribute("height");

        // Load frames info
        auto framesEl = sheetEl->FirstChildElement("Frames");
        if (framesEl) {
            sheet.rows = framesEl->UnsignedAttribute("rows");
            sheet.cols = framesEl->UnsignedAttribute("cols");
            sheet.frameWidth = framesEl->UnsignedAttribute("frameWidth");
            sheet.frameHeight = framesEl->UnsignedAttribute("frameHeight");

            // Generate frames
            for (size_t row = 0; row < sheet.rows; ++row) {
                for (size_t col = 0; col < sheet.cols; ++col) {
                    SpriteFrame frame;
                    frame.x = col * sheet.frameWidth;
                    frame.y = row * sheet.frameHeight;
                    frame.width = sheet.frameWidth;
                    frame.height = sheet.frameHeight;
                    sheet.frames.push_back(frame);
                }
            }
        }

        // Load markers
        for (auto markerEl = sheetEl->FirstChildElement("Marker"); markerEl; markerEl = markerEl->NextSiblingElement("Marker")) {
            SpriteMarker marker;
            marker.name = markerEl->Attribute("name") ? markerEl->Attribute("name") : "";
            marker.from = markerEl->UnsignedAttribute("from");
            marker.to = markerEl->UnsignedAttribute("to");

            if (!marker.name.empty()) {
                sheet.markers[marker.name] = marker;
            }
        }

        // Load layers
        for (auto layerEl = sheetEl->FirstChildElement("Layer"); layerEl; layerEl = layerEl->NextSiblingElement("Layer")) {
            SpriteLayer layer;
            layer.name = layerEl->Attribute("name") ? layerEl->Attribute("name") : "";
            layer.opacity = layerEl->IntAttribute("opacity");

            if (!layer.name.empty()) {
                sheet.layers[layer.name] = layer;
            }
        }

        // Use the sheet name from XML as the key
        if (!sheetName.empty()) {
            sprite.sheets[sheetName] = sheet;
        }
    }

    return sprite;
}

Rectangle Sprite::GetMarkerClip(const std::string& markerName) const {
    // Check if markerName contains a sheet specification (e.g., "idle/down")
    size_t slashPos = markerName.find('/');
    if (slashPos != std::string::npos) {
        // Format: "sheetName/markerName"
        std::string sheetName = markerName.substr(0, slashPos);
        std::string actualMarkerName = markerName.substr(slashPos + 1);

        auto sheetIt = sheets.find(sheetName);
        if (sheetIt != sheets.end()) {
            const SpriteSheet& sheet = sheetIt->second;
            auto markerIt = sheet.markers.find(actualMarkerName);
            if (markerIt != sheet.markers.end()) {
                const SpriteMarker& marker = markerIt->second;
                if (marker.from < sheet.frames.size()) {
                    const SpriteFrame& frame = sheet.frames[marker.from];
                    return Rectangle{
                        static_cast<float>(frame.x),
                        static_cast<float>(frame.y),
                        static_cast<float>(frame.width),
                        static_cast<float>(frame.height)};
                }
            }
        }
    } else {
        // Fallback: search all sheets for the marker (existing behavior)
        for (const auto& [sheetName, sheet] : sheets) {
            auto markerIt = sheet.markers.find(markerName);
            if (markerIt != sheet.markers.end()) {
                const SpriteMarker& marker = markerIt->second;
                if (marker.from < sheet.frames.size()) {
                    const SpriteFrame& frame = sheet.frames[marker.from];
                    return Rectangle{
                        static_cast<float>(frame.x),
                        static_cast<float>(frame.y),
                        static_cast<float>(frame.width),
                        static_cast<float>(frame.height)};
                }
            }
        }
    }
    return Rectangle{0, 0, 0, 0};
}

std::string Sprite::GetMarkerTextureName(const std::string& markerName) const {
    // Check if markerName contains a sheet specification (e.g., "idle/down")
    size_t slashPos = markerName.find('/');
    if (slashPos != std::string::npos) {
        // Format: "sheetName/markerName"
        std::string sheetName = markerName.substr(0, slashPos);
        std::string actualMarkerName = markerName.substr(slashPos + 1);

        auto sheetIt = sheets.find(sheetName);
        if (sheetIt != sheets.end()) {
            const SpriteSheet& sheet = sheetIt->second;
            auto markerIt = sheet.markers.find(actualMarkerName);
            if (markerIt != sheet.markers.end()) {
                return sheet.file;
            }
        }
    } else {
        // Fallback: search all sheets for the marker (existing behavior)
        for (const auto& [sheetName, sheet] : sheets) {
            auto markerIt = sheet.markers.find(markerName);
            if (markerIt != sheet.markers.end()) {
                return sheet.file;
            }
        }
    }
    return "";
}

int Sprite::GetMarkerFrameCount(const std::string& markerName) const {
    // Check if markerName contains a sheet specification (e.g., "idle/down")
    size_t slashPos = markerName.find('/');
    if (slashPos != std::string::npos) {
        // Format: "sheetName/markerName"
        std::string sheetName = markerName.substr(0, slashPos);
        std::string actualMarkerName = markerName.substr(slashPos + 1);

        auto sheetIt = sheets.find(sheetName);
        if (sheetIt != sheets.end()) {
            const SpriteSheet& sheet = sheetIt->second;
            auto markerIt = sheet.markers.find(actualMarkerName);
            if (markerIt != sheet.markers.end()) {
                const SpriteMarker& marker = markerIt->second;
                return (marker.to - marker.from) + 1;
            }
        }
    } else {
        // Fallback: search all sheets for the marker
        for (const auto& [sheetName, sheet] : sheets) {
            auto markerIt = sheet.markers.find(markerName);
            if (markerIt != sheet.markers.end()) {
                const SpriteMarker& marker = markerIt->second;
                return (marker.to - marker.from) + 1;
            }
        }
    }
    return 0;
}

Rectangle Sprite::GetMarkerFrameClip(const std::string& markerName, int frameIndex) const {
    // Check if markerName contains a sheet specification (e.g., "idle/down")
    size_t slashPos = markerName.find('/');
    if (slashPos != std::string::npos) {
        // Format: "sheetName/markerName"
        std::string sheetName = markerName.substr(0, slashPos);
        std::string actualMarkerName = markerName.substr(slashPos + 1);

        auto sheetIt = sheets.find(sheetName);
        if (sheetIt != sheets.end()) {
            const SpriteSheet& sheet = sheetIt->second;
            auto markerIt = sheet.markers.find(actualMarkerName);
            if (markerIt != sheet.markers.end()) {
                const SpriteMarker& marker = markerIt->second;
                int actualFrameIndex = marker.from + frameIndex;
                if (actualFrameIndex <= marker.to && actualFrameIndex < sheet.frames.size()) {
                    const SpriteFrame& frame = sheet.frames[actualFrameIndex];
                    return Rectangle{
                        static_cast<float>(frame.x),
                        static_cast<float>(frame.y),
                        static_cast<float>(frame.width),
                        static_cast<float>(frame.height)};
                }
            }
        }
    } else {
        // Fallback: search all sheets for the marker
        for (const auto& [sheetName, sheet] : sheets) {
            auto markerIt = sheet.markers.find(markerName);
            if (markerIt != sheet.markers.end()) {
                const SpriteMarker& marker = markerIt->second;
                int actualFrameIndex = marker.from + frameIndex;
                if (actualFrameIndex <= marker.to && actualFrameIndex < sheet.frames.size()) {
                    const SpriteFrame& frame = sheet.frames[actualFrameIndex];
                    return Rectangle{
                        static_cast<float>(frame.x),
                        static_cast<float>(frame.y),
                        static_cast<float>(frame.width),
                        static_cast<float>(frame.height)};
                }
            }
        }
    }
    return Rectangle{0, 0, 0, 0};
}

std::pair<int, int> Sprite::GetMarkerFrameRange(const std::string& markerName) const {
    // Check if markerName contains a sheet specification (e.g., "idle/down")
    size_t slashPos = markerName.find('/');
    if (slashPos != std::string::npos) {
        // Format: "sheetName/markerName"
        std::string sheetName = markerName.substr(0, slashPos);
        std::string actualMarkerName = markerName.substr(slashPos + 1);

        auto sheetIt = sheets.find(sheetName);
        if (sheetIt != sheets.end()) {
            const SpriteSheet& sheet = sheetIt->second;
            auto markerIt = sheet.markers.find(actualMarkerName);
            if (markerIt != sheet.markers.end()) {
                const SpriteMarker& marker = markerIt->second;
                return {static_cast<int>(marker.from), static_cast<int>(marker.to)};
            }
        }
    } else {
        // Fallback: search all sheets for the marker
        for (const auto& [sheetName, sheet] : sheets) {
            auto markerIt = sheet.markers.find(markerName);
            if (markerIt != sheet.markers.end()) {
                const SpriteMarker& marker = markerIt->second;
                return {static_cast<int>(marker.from), static_cast<int>(marker.to)};
            }
        }
    }
    return {0, 0};
}
}  // namespace Elysium

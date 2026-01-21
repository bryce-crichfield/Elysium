#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "raylib.h"

namespace Elysium {
enum class SpriteDirection {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

struct SpriteFrame {
    size_t x, y, width, height;
};

struct SpriteMarker {
    std::string name;
    size_t to, from;
};

struct SpriteLayer {
    std::string name;
    int opacity;
};

struct SpriteSheet {
    std::string file;
    size_t width, height;
    size_t rows, cols, frameWidth, frameHeight;
    std::vector<SpriteFrame> frames;
    std::unordered_map<std::string, SpriteLayer> layers;
    std::unordered_map<std::string, SpriteMarker> markers;
};

struct Sprite {
    std::string name;

    std::unordered_map<std::string, SpriteSheet> sheets;

    static Sprite LoadFromXml(const std::string& path);

    Rectangle GetMarkerClip(const std::string& markerName) const;
    std::string GetMarkerTextureName(const std::string& markerName) const;
    int GetMarkerFrameCount(const std::string& markerName) const;
    Rectangle GetMarkerFrameClip(const std::string& markerName, int frameIndex) const;
    std::pair<int, int> GetMarkerFrameRange(const std::string& markerName) const;
};
};  // namespace Elysium

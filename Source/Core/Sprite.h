#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "raylib.h"

namespace Elysium {

struct SpriteSequence {
    std::string name;
    std::vector<size_t> indices;
};

struct SpriteSheet {
    std::string name;
    std::string path;

    size_t rows, cols;
    size_t width, height;

    std::unordered_map<std::string, SpriteSequence> sequences;
};

struct Sprite {
    std::string name = "";
    float originX = 0.5f;   // normalized pivot: 0 = left,   1 = right  (default: center)
    float originY = 0.5f;   // normalized pivot: 0 = top,    1 = bottom (default: center)
    std::unordered_map<std::string, SpriteSheet> sheets;

    static Sprite LoadFromXml(const std::string& path);
};

};  // namespace Elysium

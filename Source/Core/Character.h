#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Elysium {

struct CharacterStat {
    std::string name;
    int value    = 0;
    int maxValue = 0;
};

struct CharacterItem {
    std::string slot;       // e.g. "weapon", "armor", "offhand"
    std::string id;         // item identifier
    int         quantity = 1;
};

struct Character {
    std::string name;
    std::string portrait;   // relative asset path, e.g. "Textures/Portraits/Archer128.jpg"

    std::unordered_map<std::string, CharacterStat> stats;
    std::vector<CharacterItem>                      inventory;
    std::vector<std::string>                        abilities;

    static Character LoadFromXml(const std::string& filepath);
};

}  // namespace Elysium

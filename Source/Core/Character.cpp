#include "Core/Character.h"
#include "tinyxml2.h"
#include <stdexcept>

using namespace tinyxml2;

namespace Elysium {

Character Character::LoadFromXml(const std::string& filepath) {
    XMLDocument doc;
    if (doc.LoadFile(filepath.c_str()) != XML_SUCCESS) {
        throw std::runtime_error("Failed to load Character XML: " + filepath);
    }

    auto* root = doc.FirstChildElement("Character");
    if (!root) {
        throw std::runtime_error("No <Character> root element in: " + filepath);
    }

    Character c;
    if (const char* name = root->Attribute("name")) c.name = name;

    // Portrait
    if (auto* portEl = root->FirstChildElement("Portrait")) {
        if (const char* path = portEl->Attribute("path")) c.portrait = path;
    }

    // Stats
    if (auto* statsEl = root->FirstChildElement("Stats")) {
        for (auto* statEl = statsEl->FirstChildElement("Stat");
             statEl; statEl = statEl->NextSiblingElement("Stat")) {
            const char* statName = statEl->Attribute("name");
            if (!statName) continue;
            CharacterStat stat;
            stat.name     = statName;
            stat.value    = statEl->IntAttribute("value", 0);
            stat.maxValue = statEl->IntAttribute("max", stat.value);
            c.stats[stat.name] = stat;
        }
    }

    // Inventory
    if (auto* invEl = root->FirstChildElement("Inventory")) {
        for (auto* itemEl = invEl->FirstChildElement("Item");
             itemEl; itemEl = itemEl->NextSiblingElement("Item")) {
            CharacterItem item;
            if (const char* slot = itemEl->Attribute("slot")) item.slot = slot;
            if (const char* id   = itemEl->Attribute("id"))   item.id   = id;
            item.quantity = itemEl->IntAttribute("quantity", 1);
            c.inventory.push_back(item);
        }
    }

    // Abilities
    if (auto* abEl = root->FirstChildElement("Abilities")) {
        for (auto* abilEl = abEl->FirstChildElement("Ability");
             abilEl; abilEl = abilEl->NextSiblingElement("Ability")) {
            if (const char* id = abilEl->Attribute("id")) c.abilities.push_back(id);
        }
    }

    return c;
}

}  // namespace Elysium

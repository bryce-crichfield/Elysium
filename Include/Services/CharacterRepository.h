#pragma once

#include <vector>
#include <string>
#include <optional>

namespace Elysium::Services {

class PersistenceService;

// Data transfer objects
struct ArchetypeData {
    int id;
    std::string name;
    int str_base;
    int int_base;
    int agi_base;
    float str_growth;
    float int_growth;
    float agi_growth;
};

struct ItemData {
    int id;
    std::string name;
    std::string type;
    int health;
    int weapon_damage;
    int spell_power;
    int armor;
    int ward;
    int mana;
    int stamina;
};

struct SpellData {
    int id;
    std::string name;
    std::string cost_type;
    int cost_amount;
};

struct CharacterData {
    int id;
    std::string name;
    int archetype_id;
    std::string archetype_name;
    int level;
};

struct InventoryItemData {
    std::string name;
    std::string type;
    int count;
};

// Stats calculation result
struct CalculatedStats {
    float strength;
    float intelligence;
    float agility;
    int health;
    int mana;
};

// Repository with basic business logic
class CharacterRepository {
public:
    explicit CharacterRepository(PersistenceService* persistenceService);
    ~CharacterRepository() = default;

    // Character CRUD
    std::vector<CharacterData> GetAllCharacters();
    std::optional<CharacterData> GetCharacter(int id);
    int CreateCharacter(const std::string& name, int archetypeId, int level);
    bool UpdateCharacter(int id, const std::string& name, int archetypeId, int level);
    bool DeleteCharacter(int id);

    // Archetype CRUD
    std::vector<ArchetypeData> GetAllArchetypes();
    std::optional<ArchetypeData> GetArchetype(int id);
    std::optional<ArchetypeData> GetArchetypeByName(const std::string& name);
    int CreateArchetype(const std::string& name, int strBase, int intBase, int agiBase, 
                       float strGrowth, float intGrowth, float agiGrowth);
    bool UpdateArchetype(int id, const std::string& name, int strBase, int intBase, int agiBase,
                        float strGrowth, float intGrowth, float agiGrowth);
    bool DeleteArchetype(int id);

    // Item CRUD
    std::vector<ItemData> GetAllItems();
    std::optional<ItemData> GetItem(int id);
    int CreateItem(const std::string& name, const std::string& type, int health, int weaponDamage,
                   int spellPower, int armor, int ward, int mana, int stamina);
    bool UpdateItem(int id, const std::string& name, const std::string& type, int health, int weaponDamage,
                   int spellPower, int armor, int ward, int mana, int stamina);
    bool DeleteItem(int id);

    // Spell CRUD
    std::vector<SpellData> GetAllSpells();
    std::optional<SpellData> GetSpell(int id);
    int CreateSpell(const std::string& name, const std::string& costType, int costAmount);
    bool UpdateSpell(int id, const std::string& name, const std::string& costType, int costAmount);
    bool DeleteSpell(int id);

    // Inventory operations
    std::vector<InventoryItemData> GetCharacterInventory(int characterId);
    bool AddItemToInventory(int characterId, int itemId, int count = 1);
    bool RemoveItemFromInventory(int characterId, int itemId, int count = 1);

    // Spellbook operations
    std::vector<SpellData> GetCharacterSpells(int characterId);
    bool AddSpellToSpellbook(int characterId, int spellId);
    bool RemoveSpellFromSpellbook(int characterId, int spellId);

    // === BUSINESS LOGIC ===
    // Stats calculation
    CalculatedStats CalculateCharacterStats(int characterId);
    CalculatedStats CalculateCharacterStats(const std::string& archetypeName, int level);
    
    // Helper methods
    int FindArchetypeIdByName(const std::string& name);

private:
    PersistenceService* persistenceService_;
};

} // namespace Elysium::Services
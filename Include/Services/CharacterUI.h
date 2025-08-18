#pragma once

#include "Services/CharacterRepository.h"
#include <memory>
#include <string>
#include <cstring>

namespace Elysium::Services {

// Form state data structures
struct CharacterFormData {
    char name[128] = "";
    int archetypeIndex = 0;
    int level = 10;
    
    void Clear() {
        name[0] = '\0';
        archetypeIndex = 0;
        level = 10;
    }
    
    void LoadFrom(const CharacterData& character, const std::vector<ArchetypeData>& archetypes) {
        strncpy(name, character.name.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        level = character.level;
        
        // Find archetype index
        for (size_t i = 0; i < archetypes.size(); ++i) {
            if (archetypes[i].id == character.archetype_id) {
                archetypeIndex = static_cast<int>(i);
                break;
            }
        }
    }
};

struct ArchetypeFormData {
    char name[128] = "";
    int strBase = 10;
    int intBase = 10;
    int agiBase = 10;
    float strGrowth = 2.0f;
    float intGrowth = 2.0f;
    float agiGrowth = 2.0f;
    
    void Clear() {
        name[0] = '\0';
        strBase = intBase = agiBase = 10;
        strGrowth = intGrowth = agiGrowth = 2.0f;
    }
    
    void LoadFrom(const ArchetypeData& archetype) {
        strncpy(name, archetype.name.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        strBase = archetype.str_base;
        intBase = archetype.int_base;
        agiBase = archetype.agi_base;
        strGrowth = archetype.str_growth;
        intGrowth = archetype.int_growth;
        agiGrowth = archetype.agi_growth;
    }
};

struct ItemFormData {
    char name[128] = "";
    char type[64] = "weapon";
    int health = 0;
    int weaponDamage = 0;
    int spellPower = 0;
    int armor = 0;
    int ward = 0;
    int mana = 0;
    int stamina = 0;
    
    void Clear() {
        name[0] = '\0';
        strcpy(type, "weapon");
        health = weaponDamage = spellPower = armor = ward = mana = stamina = 0;
    }
    
    void LoadFrom(const ItemData& item) {
        strncpy(name, item.name.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        strncpy(type, item.type.c_str(), sizeof(type) - 1);
        type[sizeof(type) - 1] = '\0';
        health = item.health;
        weaponDamage = item.weapon_damage;
        spellPower = item.spell_power;
        armor = item.armor;
        ward = item.ward;
        mana = item.mana;
        stamina = item.stamina;
    }
};

struct SpellFormData {
    char name[128] = "";
    char costType[64] = "mana";
    int costAmount = 10;
    
    void Clear() {
        name[0] = '\0';
        strcpy(costType, "mana");
        costAmount = 10;
    }
    
    void LoadFrom(const SpellData& spell) {
        strncpy(name, spell.name.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        strncpy(costType, spell.cost_type.c_str(), sizeof(costType) - 1);
        costType[sizeof(costType) - 1] = '\0';
        costAmount = spell.cost_amount;
    }
};

// Clean UI class with hidden implementation
class CharacterUI {
public:
    explicit CharacterUI(std::shared_ptr<CharacterRepository> repository);
    ~CharacterUI() = default;

    // Public interface - just render
    void Draw();

private:
    // === DATA ===
    std::shared_ptr<CharacterRepository> repository_;
    
    // Current selections
    int currentCharacterId_ = -1;
    int currentArchetypeId_ = -1;
    int currentItemId_ = -1;
    int currentSpellId_ = -1;
    
    // Tab state
    int selectedTab_ = 0;
    int characterSubTab_ = 0;
    
    // Form data structures
    CharacterFormData characterForm_;
    ArchetypeFormData archetypeForm_;
    ItemFormData itemForm_;
    SpellFormData spellForm_;
    
    // Cached data
    std::vector<CharacterData> characters_;
    std::vector<ArchetypeData> archetypes_;
    std::vector<ItemData> items_;
    std::vector<SpellData> spells_;
    std::vector<InventoryItemData> currentInventory_;
    std::vector<SpellData> currentSpellbook_;
    
    std::string statusMessage_;
    
    // === UI RENDERING ===
    void DrawCharactersTab();
    void DrawCharacterAttributesTab();
    void DrawCharacterInventoryTab();
    void DrawCharacterSpellsTab();
    void DrawArchetypesTab();
    void DrawItemsTab();
    void DrawSpellsTab();
    void DrawExportTab();
    
    // === DATA OPERATIONS ===
    void RefreshData();
    void RefreshCharacters();
    void RefreshArchetypes();
    void RefreshItems();
    void RefreshSpells();
    void RefreshCurrentInventory();
    void RefreshCurrentSpellbook();
    
    // === SELECTIONS ===
    void SelectCharacter(int characterId);
    void SelectArchetype(int archetypeId);
    void SelectItem(int itemId);
    void SelectSpell(int spellId);
    
    // === CRUD OPERATIONS ===
    void SaveCharacter();
    void SaveArchetype();
    void SaveItem();
    void SaveSpell();
    
    void DeleteCharacter();
    void DeleteArchetype();
    void DeleteItem();
    void DeleteSpell();
    
    void NewCharacter();
    void NewArchetype();
    void NewItem();
    void NewSpell();
    
    // === HELPER METHODS ===
    void AddItemToInventory();
    void AddSpellToSpellbook();
    void ExportToJson();
};

} // namespace Elysium::Services
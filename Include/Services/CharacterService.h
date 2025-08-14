#pragma once

#include <memory>
#include <vector>
#include <string>

namespace Elysium::Services {

class PersistenceService;

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

class CharacterService {
public:
    CharacterService(PersistenceService* persistenceService);
    ~CharacterService();

    void Draw();

private:
    PersistenceService* persistenceService_;
    
    // Current selections
    int currentCharacterId_;
    int currentArchetypeId_;
    int currentItemId_;
    int currentSpellId_;
    
    // Tab state
    int selectedTab_;
    int characterSubTab_;
    
    // Form data
    char characterName_[128];
    char archetypeName_[128];
    char itemName_[128];
    char spellName_[128];
    int characterLevel_;
    int selectedArchetypeIndex_;
    int selectedItemIndex_;
    int selectedSpellIndex_;
    
    // Archetype form data
    int strBase_;
    int intBase_;
    int agiBase_;
    float strGrowth_;
    float intGrowth_;
    float agiGrowth_;
    
    // Item form data
    char itemType_[64];
    int itemHealth_;
    int itemWeaponDamage_;
    int itemSpellPower_;
    int itemArmor_;
    
    // Spell form data
    char spellCostType_[64];
    int spellCostAmount_;
    
    // Cache data
    std::vector<ArchetypeData> archetypes_;
    std::vector<ItemData> items_;
    std::vector<SpellData> spells_;
    std::vector<CharacterData> characters_;
    std::vector<InventoryItemData> currentInventory_;
    std::vector<SpellData> currentSpellbook_;
    
    // Helper methods
    void RefreshData();
    void RefreshArchetypes();
    void RefreshItems();
    void RefreshSpells();
    void RefreshCharacters();
    void RefreshCurrentInventory();
    void RefreshCurrentSpellbook();
    
    void DrawCharactersTab();
    void DrawCharacterAttributesTab();
    void DrawCharacterInventoryTab();
    void DrawCharacterSpellsTab();
    void DrawArchetypesTab();
    void DrawItemsTab();
    void DrawSpellsTab();
    void DrawExportTab();
    
    void SelectCharacter(int characterId);
    void SelectArchetype(int archetypeId);
    void SelectItem(int itemId);
    void SelectSpell(int spellId);
    
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
    
    void AddItemToInventory();
    void AddSpellToSpellbook();
    
    void UpdateCharacterPreview();
    void CalculateStats(const std::string& archetypeName, int level, float& str, float& intel, float& agi, int& health, int& mana);
    
    void ExportToJson();
    
    std::string statusMessage_;
};

} // namespace Elysium::Services
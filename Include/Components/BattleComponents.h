#pragma once

#include "raylib.h"
#include <string>
#include <vector>

namespace Elysium {

struct HealthComponent {
    int currentHealth;
    int maxHealth;
    bool isAlive;
    
    HealthComponent(int maxHp = 100) 
        : currentHealth(maxHp), maxHealth(maxHp), isAlive(true) {}
    
    void TakeDamage(int damage) {
        currentHealth = std::max(0, currentHealth - damage);
        if (currentHealth <= 0) {
            isAlive = false;
        }
    }
    
    void Heal(int amount) {
        currentHealth = std::min(maxHealth, currentHealth + amount);
        if (currentHealth > 0) {
            isAlive = true;
        }
    }
    
    float GetHealthPercentage() const {
        return maxHealth > 0 ? static_cast<float>(currentHealth) / maxHealth : 0.0f;
    }
};

struct ManaComponent {
    int currentMana;
    int maxMana;
    
    ManaComponent(int maxMp = 50) 
        : currentMana(maxMp), maxMana(maxMp) {}
    
    bool CanCast(int cost) const {
        return currentMana >= cost;
    }
    
    bool SpendMana(int cost) {
        if (CanCast(cost)) {
            currentMana -= cost;
            return true;
        }
        return false;
    }
    
    void RestoreMana(int amount) {
        currentMana = std::min(maxMana, currentMana + amount);
    }
    
    float GetManaPercentage() const {
        return maxMana > 0 ? static_cast<float>(currentMana) / maxMana : 0.0f;
    }
};

struct StatsComponent {
    int strength;
    int intelligence;
    int agility;
    int level;
    
    StatsComponent(int str = 10, int intel = 10, int agi = 10, int lvl = 1)
        : strength(str), intelligence(intel), agility(agi), level(lvl) {}
    
    int GetAttackPower() const { return strength + level; }
    int GetSpellPower() const { return intelligence + level; }
    int GetSpeed() const { return agility; }
    int GetDefense() const { return strength / 2; }
};

struct BattleUnitComponent {
    int characterId;
    bool hasActedThisTurn;
    bool canMove;
    bool canAttack;
    bool canCastSpells;
    bool canUseItems;
    int movementRange;
    int attackRange;
    
    BattleUnitComponent(int charId = -1)
        : characterId(charId)
        , hasActedThisTurn(false)
        , canMove(true)
        , canAttack(true)
        , canCastSpells(true)
        , canUseItems(true)
        , movementRange(3)
        , attackRange(1) {}
    
    void StartTurn() {
        hasActedThisTurn = false;
        canMove = true;
        canAttack = true;
        canCastSpells = true;
        canUseItems = true;
    }
    
    void EndTurn() {
        hasActedThisTurn = true;
        canMove = false;
        canAttack = false;
        canCastSpells = false;
        canUseItems = false;
    }
};

struct InventoryComponent {
    struct Item {
        int itemId;
        std::string name;
        std::string type;
        int count;
        
        Item(int id = -1, const std::string& n = "", const std::string& t = "", int c = 1)
            : itemId(id), name(n), type(t), count(c) {}
    };
    
    std::vector<Item> items;
    int maxSlots;
    
    InventoryComponent(int maxItems = 20) : maxSlots(maxItems) {}
    
    bool AddItem(const Item& item) {
        auto it = std::find_if(items.begin(), items.end(), [&](const Item& existing) {
            return existing.itemId == item.itemId;
        });
        
        if (it != items.end()) {
            it->count += item.count;
            return true;
        } else if (items.size() < maxSlots) {
            items.push_back(item);
            return true;
        }
        return false;
    }
    
    bool RemoveItem(int itemId, int count = 1) {
        auto it = std::find_if(items.begin(), items.end(), [itemId](const Item& item) {
            return item.itemId == itemId;
        });
        
        if (it != items.end() && it->count >= count) {
            it->count -= count;
            if (it->count <= 0) {
                items.erase(it);
            }
            return true;
        }
        return false;
    }
    
    const Item* GetItem(int itemId) const {
        auto it = std::find_if(items.begin(), items.end(), [itemId](const Item& item) {
            return item.itemId == itemId;
        });
        return it != items.end() ? &(*it) : nullptr;
    }
};

struct SpellbookComponent {
    struct Spell {
        int spellId;
        std::string name;
        std::string costType;
        int costAmount;
        
        Spell(int id = -1, const std::string& n = "", const std::string& ct = "mana", int ca = 10)
            : spellId(id), name(n), costType(ct), costAmount(ca) {}
    };
    
    std::vector<Spell> spells;
    
    bool HasSpell(int spellId) const {
        return std::find_if(spells.begin(), spells.end(), [spellId](const Spell& spell) {
            return spell.spellId == spellId;
        }) != spells.end();
    }
    
    const Spell* GetSpell(int spellId) const {
        auto it = std::find_if(spells.begin(), spells.end(), [spellId](const Spell& spell) {
            return spell.spellId == spellId;
        });
        return it != spells.end() ? &(*it) : nullptr;
    }
    
    void AddSpell(const Spell& spell) {
        if (!HasSpell(spell.spellId)) {
            spells.push_back(spell);
        }
    }
};

struct GridPositionComponent {
    int gridX;
    int gridY;
    bool isValidPosition;
    
    GridPositionComponent(int x = 0, int y = 0) 
        : gridX(x), gridY(y), isValidPosition(true) {}
    
    Vector2 GetWorldPosition(float tileSize = 32.0f) const {
        return { gridX * tileSize, gridY * tileSize };
    }
    
    void SetFromWorldPosition(Vector2 worldPos, float tileSize = 32.0f) {
        gridX = static_cast<int>(worldPos.x / tileSize);
        gridY = static_cast<int>(worldPos.y / tileSize);
    }
};

struct TargetableComponent {
    bool canBeTargeted;
    bool isHostile;
    std::vector<std::string> targetableBy;
    
    TargetableComponent(bool targetable = true, bool hostile = false)
        : canBeTargeted(targetable), isHostile(hostile) {}
};

} // namespace Elysium
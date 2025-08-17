#include "Services/BattleEntityService.h"
#include "Services/PersistenceService.h"
#include "Application.h"
#include "Component.h"
#include <SQLiteCpp/SQLiteCpp.h>

namespace Elysium::Services {

BattleEntityService::BattleEntityService() : componentsRegistered(false) {
}

BattleEntityService::~BattleEntityService() {
}

void BattleEntityService::RegisterBattleComponents(World* world) {
    if (componentsRegistered || !world) return;
    
    world->RegisterComponent<HealthComponent>();
    world->RegisterComponent<ManaComponent>();
    world->RegisterComponent<StatsComponent>();
    world->RegisterComponent<BattleUnitComponent>();
    world->RegisterComponent<InventoryComponent>();
    world->RegisterComponent<SpellbookComponent>();
    world->RegisterComponent<GridPositionComponent>();
    world->RegisterComponent<TargetableComponent>();
    world->RegisterComponent<TeamComponent>();
    world->RegisterComponent<PositionComponent>();
    
    componentsRegistered = true;
}

Entity BattleEntityService::CreateBattleEntityFromCharacter(const CharacterData& characterData, World* world, bool isPlayerUnit) {
    if (!world) return INVALID_ENTITY;
    
    RegisterBattleComponents(world);
    
    Entity entity = world->CreateEntity("BattleUnit_" + characterData.name);
    characterIdToEntity[characterData.id] = entity;
    
    LoadCharacterStats(entity, characterData, world);
    LoadCharacterInventory(entity, characterData.id, world);
    LoadCharacterSpellbook(entity, characterData.id, world);
    
    world->AddComponent(entity, BattleUnitComponent(characterData.id));
    world->AddComponent(entity, TargetableComponent(true, !isPlayerUnit));
    world->AddComponent(entity, TeamComponent(isPlayerUnit ? 0 : 1));
    world->AddComponent(entity, GridPositionComponent(0, 0));
    world->AddComponent(entity, PositionComponent(0.0f, 0.0f));
    
    return entity;
}

void BattleEntityService::LoadCharacterStats(Entity entity, const CharacterData& characterData, World* world) {
    if (!world) return;
    
    int str, intel, agi, health, mana;
    CalculateStatsFromArchetype(characterData.archetype_name, characterData.level, str, intel, agi, health, mana);
    
    world->AddComponent(entity, StatsComponent(str, intel, agi, characterData.level));
    world->AddComponent(entity, HealthComponent(health));
    world->AddComponent(entity, ManaComponent(mana));
}

void BattleEntityService::LoadCharacterInventory(Entity entity, int characterId, World* world) {
    if (!world) return;
    
    InventoryComponent inventory;
    auto& persistenceService = Application::GetInstance().GetPersistenceService();
    
    if (!persistenceService.GetDatabase()) return;
    
    try {
        SQLite::Statement query(*persistenceService.GetDatabase(),
            "SELECT i.id, i.name, i.type, COUNT(*) as count "
            "FROM inventory_items ii "
            "JOIN inventories inv ON ii.inventory_id = inv.id "
            "JOIN items i ON ii.item_id = i.id "
            "WHERE inv.character_id = ? "
            "GROUP BY i.id, i.name, i.type "
            "ORDER BY i.name");
        
        query.bind(1, characterId);
        
        while (query.executeStep()) {
            InventoryComponent::Item item;
            item.itemId = query.getColumn(0).getInt();
            item.name = query.getColumn(1).getText();
            item.type = query.getColumn(2).getText();
            item.count = query.getColumn(3).getInt();
            inventory.AddItem(item);
        }
    }
    catch (const std::exception& e) {
        // Handle error silently or log
    }
    
    world->AddComponent(entity, inventory);
}

void BattleEntityService::LoadCharacterSpellbook(Entity entity, int characterId, World* world) {
    if (!world) return;
    
    SpellbookComponent spellbook;
    auto& persistenceService = Application::GetInstance().GetPersistenceService();
    
    if (!persistenceService.GetDatabase()) return;
    
    try {
        SQLite::Statement query(*persistenceService.GetDatabase(),
            "SELECT s.id, s.name, s.cost_type, s.cost_amount "
            "FROM spellbook_spells ss "
            "JOIN spellbooks sb ON ss.spellbook_id = sb.id "
            "JOIN spells s ON ss.spell_id = s.id "
            "WHERE sb.character_id = ? "
            "ORDER BY s.name");
        
        query.bind(1, characterId);
        
        while (query.executeStep()) {
            SpellbookComponent::Spell spell;
            spell.spellId = query.getColumn(0).getInt();
            spell.name = query.getColumn(1).getText();
            spell.costType = query.getColumn(2).getText();
            spell.costAmount = query.getColumn(3).getInt();
            spellbook.AddSpell(spell);
        }
    }
    catch (const std::exception& e) {
        // Handle error silently or log
    }
    
    world->AddComponent(entity, spellbook);
}

void BattleEntityService::SetupBattlePosition(Entity entity, int gridX, int gridY, World* world) {
    if (!world || !world->HasComponent<GridPositionComponent>(entity)) return;
    
    auto& gridPos = world->GetComponent<GridPositionComponent>(entity);
    gridPos.gridX = gridX;
    gridPos.gridY = gridY;
    
    if (world->HasComponent<PositionComponent>(entity)) {
        auto& worldPos = world->GetComponent<PositionComponent>(entity);
        Vector2 pos = gridPos.GetWorldPosition();
        worldPos.x = pos.x;
        worldPos.y = pos.y;
    }
}

std::vector<Entity> BattleEntityService::CreatePlayerTeam(const std::vector<CharacterData>& characters, World* world) {
    std::vector<Entity> entities;
    
    for (const auto& character : characters) {
        Entity entity = CreateBattleEntityFromCharacter(character, world, true);
        if (entity != INVALID_ENTITY) {
            entities.push_back(entity);
        }
    }
    
    return entities;
}

std::vector<Entity> BattleEntityService::CreateEnemyTeam(const std::vector<CharacterData>& characters, World* world) {
    std::vector<Entity> entities;
    
    for (const auto& character : characters) {
        Entity entity = CreateBattleEntityFromCharacter(character, world, false);
        if (entity != INVALID_ENTITY) {
            entities.push_back(entity);
        }
    }
    
    return entities;
}

void BattleEntityService::ConvertCharacterIdsToEntities(const std::vector<int>& characterIds, std::vector<Entity>& entities, World* world) {
    entities.clear();
    
    for (int charId : characterIds) {
        auto it = characterIdToEntity.find(charId);
        if (it != characterIdToEntity.end()) {
            entities.push_back(it->second);
        }
    }
}

void BattleEntityService::CalculateStatsFromArchetype(const std::string& archetypeName, int level, int& str, int& intel, int& agi, int& health, int& mana) {
    auto& persistenceService = Application::GetInstance().GetPersistenceService();
    
    if (!persistenceService.GetDatabase()) {
        str = intel = agi = 10;
        health = mana = 100;
        return;
    }
    
    try {
        SQLite::Statement query(*persistenceService.GetDatabase(),
            "SELECT str_base, int_base, agi_base, str_growth, int_growth, agi_growth "
            "FROM archetypes WHERE name = ?");
        
        query.bind(1, archetypeName);
        
        if (query.executeStep()) {
            int strBase = query.getColumn(0).getInt();
            int intBase = query.getColumn(1).getInt();
            int agiBase = query.getColumn(2).getInt();
            float strGrowth = query.getColumn(3).getDouble();
            float intGrowth = query.getColumn(4).getDouble();
            float agiGrowth = query.getColumn(5).getDouble();
            
            str = strBase + static_cast<int>(level * strGrowth);
            intel = intBase + static_cast<int>(level * intGrowth);
            agi = agiBase + static_cast<int>(level * agiGrowth);
            health = str * 10 + level * 5;
            mana = intel * 8 + level * 3;
        } else {
            str = intel = agi = 10 + level;
            health = (10 + level) * 10;
            mana = (10 + level) * 8;
        }
    }
    catch (const std::exception& e) {
        str = intel = agi = 10 + level;
        health = (10 + level) * 10;
        mana = (10 + level) * 8;
    }
}

} // namespace Elysium::Services
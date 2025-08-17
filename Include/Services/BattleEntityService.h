#pragma once

#include "Entity.h"
#include "Services/CharacterService.h"
#include "Components/BattleComponents.h"
#include <vector>
#include <unordered_map>

namespace Elysium::Services {

class BattleEntityService {
public:
    BattleEntityService();
    ~BattleEntityService();
    
    Entity CreateBattleEntityFromCharacter(const CharacterData& characterData, World* world, bool isPlayerUnit = true);
    
    void LoadCharacterStats(Entity entity, const CharacterData& characterData, World* world);
    void LoadCharacterInventory(Entity entity, int characterId, World* world);
    void LoadCharacterSpellbook(Entity entity, int characterId, World* world);
    
    void SetupBattlePosition(Entity entity, int gridX, int gridY, World* world);
    
    std::vector<Entity> CreatePlayerTeam(const std::vector<CharacterData>& characters, World* world);
    std::vector<Entity> CreateEnemyTeam(const std::vector<CharacterData>& characters, World* world);
    
    void ConvertCharacterIdsToEntities(const std::vector<int>& characterIds, std::vector<Entity>& entities, World* world);
    
private:
    void CalculateStatsFromArchetype(const std::string& archetypeName, int level, int& str, int& intel, int& agi, int& health, int& mana);
    void RegisterBattleComponents(World* world);
    
    std::unordered_map<int, Entity> characterIdToEntity;
    bool componentsRegistered;
};

} // namespace Elysium::Services
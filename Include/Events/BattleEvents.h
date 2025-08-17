#pragma once

#include "raylib.h"
#include <string>
#include <vector>

namespace Elysium::Events {

struct BattleStartEvent {
    std::string battleId;
    std::vector<int> playerUnits;
    std::vector<int> enemyUnits;
};

struct BattleEndEvent {
    std::string battleId;
    bool playerVictory;
};

struct TurnStartEvent {
    int unitId;
    int turnNumber;
};

struct TurnEndEvent {
    int unitId;
    int turnNumber;
};

struct AttackEvent {
    int attackerId;
    int targetId;
    int damage;
    Vector2 attackerPosition;
    Vector2 targetPosition;
    std::string attackType;
};

struct MoveEvent {
    int unitId;
    Vector2 fromPosition;
    Vector2 toPosition;
    float movementCost;
};

struct CastSpellEvent {
    int casterId;
    std::string spellName;
    Vector2 targetPosition;
    std::vector<int> affectedUnits;
    int manaCost;
};

struct UseIt1emEvent {
    int userId;
    std::string itemName;
    Vector2 targetPosition;
    std::vector<int> affectedUnits;
};

struct UnitDefeatedEvent {
    int unitId;
    Vector2 position;
    bool isPlayerUnit;
};

struct ActionSelectedEvent {
    int unitId;
    std::string actionType;
};

} // namespace Elysium::Events

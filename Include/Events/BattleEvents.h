#pragma once

#include "Entity.h"
#include "raylib.h"
#include <string>
#include <vector>

namespace Elysium::Events {

struct BattleStartEvent {
    std::string battleId;
};

struct BattleEndEvent {
    std::string battleId;
    bool playerVictory;
};

struct TurnStartEvent {
    int teamId;
    int turnNumber;
};

struct TurnEndEvent {
    int teamId;
    int turnNumber;
};

struct AttackEvent {
    Entity entity;
    Entity target;
};

struct MoveEvent {
    Entity entity;
    Vector2 toPosition;
};

struct CastSpellEvent {
    Entity entity;
    std::string spellName;
    Vector2 targetPosition;
};

struct UseItemEvent {
    Entity user;
    std::string itemName;
    Vector2 targetPosition;
};

struct UnitKilledEvent {
    Entity unit;
};

struct ActionSelectedEvent {
    Entity unit;
    std::string action;
};

} // namespace Elysium::Events

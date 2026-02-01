#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct UnitComponent {
        bool hasActedThisTurn;
        bool canMove;
        bool canAttack;
        bool canCastSpells;
        bool canUseItems;

        UnitComponent()
            : hasActedThisTurn(false), canMove(true), canAttack(true), canCastSpells(true), canUseItems(true) {}

        void StartTurn() {
            hasActedThisTurn = false;
        }

        void EndTurn() {
            hasActedThisTurn = true;
        }

        static constexpr const char* Name() { return "Unit"; }
        static constexpr const char* XmlTag() { return "UnitComponent"; }

        static void LoadXml(UnitComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(UnitComponent& c, Entity e);
        static void BindLua(sol::usertype<UnitComponent>& ut);
        static void SetFromLua(UnitComponent& c, sol::object v);
    };
}

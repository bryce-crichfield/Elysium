#include "Core/Components/MovementComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"

namespace Elysium {
    static const char* StateName(MovementState s) {
        switch (s) {
            case MovementState::Moving:  return "Moving";
            case MovementState::Waiting: return "Waiting";
            case MovementState::Idle:    return "Idle";
        }
        return "Idle";
    }

    void MovementComponent::SaveXml(const MovementComponent& c, XMLBuilder& builder) {
        builder.AddElement("MovementComponent")
            .SetAttribute("state", StateName(c.state))
            .SetAttribute("goalX", c.goal.x)
            .SetAttribute("goalY", c.goal.y);
    }

    void MovementComponent::LoadXml(MovementComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        // state
        const char* stateStr = el->Attribute("state");
        if (stateStr) {
            if      (strcmp(stateStr, "Moving")  == 0) c.state = MovementState::Moving;
            else if (strcmp(stateStr, "Waiting") == 0) c.state = MovementState::Waiting;
            else                                        c.state = MovementState::Idle;
        }

        // goal
        c.goal.x = el->FloatAttribute("goalX", 0.0f);
        c.goal.y = el->FloatAttribute("goalY", 0.0f);

        // last position — on load, treat as current so stuck detection
        // doesn't immediately fire on the first check interval
        c.lastPosition.x = el->FloatAttribute("lastPosX", c.goal.x);
        c.lastPosition.y = el->FloatAttribute("lastPosY", c.goal.y);

        c.waitTimeMs          = el->IntAttribute("waitTimeMs",       0);
        c.stuckRetryCount     = el->IntAttribute("stuckRetryCount",  0);
        c.currentWaypointIndex = el->IntAttribute("waypointIndex",   0);
        c.stuckCheckAccumMs   = 0; // always reset on load — don't persist mid-frame state

        // waypoints — usually not persisted (replanned at runtime),
        // but support it for editor-placed patrol paths etc.
        VisitElement(el, "Waypoints", [&](tinyxml2::XMLElement* xmlWaypoints) {
            ForEachElement(xmlWaypoints, "Waypoint", [&](tinyxml2::XMLElement* xmlWaypoint) {
                Vector2 wp;
                wp.x = xmlWaypoint->FloatAttribute("x", 0.0f);
                wp.y = xmlWaypoint->FloatAttribute("y", 0.0f);
                c.waypoints.push_back(wp);
            });
        });
    }

    void MovementComponent::BindLua(sol::usertype<MovementComponent>& ut) {
        ut["state"] = sol::property(
            [](MovementComponent& c) { return static_cast<int>(c.state); },
            [](MovementComponent& c, int v) { c.state = static_cast<MovementState>(v); }
        );

        ut["goal"]              = &MovementComponent::goal;
        ut["waitTimeMs"]        = &MovementComponent::waitTimeMs;
        ut["stuckRetryCount"]   = &MovementComponent::stuckRetryCount;
        ut["waypointIndex"]     = &MovementComponent::currentWaypointIndex;
        ut["lastPosition"]      = &MovementComponent::lastPosition;

        // expose state enum values as constants on the type
        ut["Idle"]    = sol::var(static_cast<int>(MovementState::Idle));
        ut["Moving"]  = sol::var(static_cast<int>(MovementState::Moving));
        ut["Waiting"] = sol::var(static_cast<int>(MovementState::Waiting));

        ut["ClearWaypoints"] = [](MovementComponent& c) {
            c.waypoints.clear();
            c.currentWaypointIndex = 0;
        };
        ut["AddWaypoint"] = [](MovementComponent& c, float x, float y) {
            c.waypoints.push_back({x, y});
        };
        ut["GetWaypointCount"] = [](MovementComponent& c) {
            return c.waypoints.size();
        };
        ut["GetWaypoint"] = [](MovementComponent& c, int i) -> Vector2 {
            if (i >= 0 && i < static_cast<int>(c.waypoints.size()))
                return c.waypoints[i];
            return {0.0f, 0.0f};
        };
    }

    void MovementComponent::SetFromLua(MovementComponent& c, sol::object v) {
        if (!v.is<sol::table>()) return;
        sol::table t = v.as<sol::table>();

        // state — accept either int or string
        if (auto s = t.get<sol::optional<int>>("state"))
            c.state = static_cast<MovementState>(*s);
        else if (auto s = t.get<sol::optional<std::string>>("state")) {
            if      (*s == "Moving")  c.state = MovementState::Moving;
            else if (*s == "Waiting") c.state = MovementState::Waiting;
            else                      c.state = MovementState::Idle;
        }

        if (auto g = t.get<sol::optional<sol::table>>("goal")) {
            c.goal.x = g->get_or("x", c.goal.x);
            c.goal.y = g->get_or("y", c.goal.y);
        }

        c.waitTimeMs      = t.get_or("waitTimeMs",      c.waitTimeMs);
        c.stuckRetryCount = t.get_or("stuckRetryCount", c.stuckRetryCount);

        // waypoints table: { {x=10,y=20}, {x=30,y=40}, ... }
        if (auto wps = t.get<sol::optional<sol::table>>("waypoints")) {
            c.waypoints.clear();
            c.currentWaypointIndex = 0;
            wps->for_each([&](sol::object, sol::object val) {
                if (val.is<sol::table>()) {
                    sol::table wp = val.as<sol::table>();
                    // TODO: Push waypoints for MovementComponent from Lua.
                    // c.waypoints.push_back({
                    //     wp.get_or("x", 0.0f),
                    //     wp.get_or("y", 0.0f)
                    // });
                }
            });
        }
    }

    REGISTER_COMPONENT(MovementComponent);
}
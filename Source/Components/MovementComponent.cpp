#include "Components/MovementComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    MovementComponent::MovementComponent(const std::vector<Vector2>& waypoints) 
        : waypoints(waypoints) {}

    void MovementComponent::AddWaypoint(const Vector2& waypoint) {
        waypoints.push_back(waypoint);
    }

    void MovementComponent::ClearWaypoints() {
        waypoints.clear();
    }

    void MovementComponent::LoadXml(MovementComponent& c, tinyxml2::XMLElement* el) {
        VisitElement(el, "Waypoints", [&](tinyxml2::XMLElement* xmlWaypoints) {
            ForEachElement(xmlWaypoints, "Waypoint", [&](tinyxml2::XMLElement* xmlWaypoint) {
                int x = xmlWaypoint->IntAttribute("x", 0);
                int y = xmlWaypoint->IntAttribute("y", 0);
                c.waypoints.push_back({(float)x, (float)y});
            });
        });
    }

    void MovementComponent::Inspect(MovementComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Speed: ");
        ImGui::DragFloat("##Speed", &c.speed, 1.0f, 0.0f, 1000.0f);
        Label("Is Moving: ");
        ImGui::Checkbox("##IsMoving", &c.isMoving);
        Label("Loop: ");
        ImGui::Checkbox("##Loop", &c.loop);
        
        ImGui::Text("Waypoints: %zu", c.waypoints.size());
        ImGui::Text("Current Waypoint: %zu", c.currentWaypointIndex);
        for (size_t i = 0; i < c.waypoints.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Waypoint %zu:", i);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat2("##Waypoint", &c.waypoints[i].x, 1.0f);
            ImGui::PopID();
        }
    }

    void MovementComponent::BindLua(sol::usertype<MovementComponent>& ut) {
        ut["speed"] = &MovementComponent::speed;
        ut["isMoving"] = &MovementComponent::isMoving;
        ut["loop"] = &MovementComponent::loop;
        ut["AddWaypoint"] = &MovementComponent::AddWaypoint;
        ut["ClearWaypoints"] = &MovementComponent::ClearWaypoints;
    }

    void MovementComponent::SetFromLua(MovementComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.speed = t.get_or("speed", c.speed);
            c.isMoving = t.get_or("isMoving", c.isMoving);
            c.loop = t.get_or("loop", c.loop);
        }
    }

    REGISTER_COMPONENT(MovementComponent);
}

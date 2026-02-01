#include "Components/DirectionComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void DirectionComponent::SetDirection(Direction newDir) {
        if (newDir != currentDirection) {
            previousDirection = currentDirection;
            currentDirection = newDir;
            hasChanged = true;
        }
    }

    void DirectionComponent::LoadXml(DirectionComponent& c, tinyxml2::XMLElement* el) {
        // No attributes to load for now
    }

    void DirectionComponent::Inspect(DirectionComponent& c, Entity e) {
        const char* directions[] = {"NONE", "UP", "DOWN", "LEFT", "RIGHT"};
        int currentDir = static_cast<int>(c.currentDirection);
        int previousDir = static_cast<int>(c.previousDirection);

        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Current Dir: ");
        if (ImGui::Combo("##CurrentDirection", &currentDir, directions, IM_ARRAYSIZE(directions))) {
            c.SetDirection(static_cast<Direction>(currentDir));
        }

        Label("Previous Dir: ");
        ImGui::Combo("##PreviousDirection", &previousDir, directions, IM_ARRAYSIZE(directions));
        Label("Has Changed: ");
        ImGui::Checkbox("##HasChanged", &c.hasChanged);
    }

    void DirectionComponent::BindLua(sol::usertype<DirectionComponent>& ut) {
        ut["currentDirection"] = &DirectionComponent::currentDirection;
        ut["previousDirection"] = &DirectionComponent::previousDirection;
        ut["hasChanged"] = &DirectionComponent::hasChanged;
        ut["SetDirection"] = &DirectionComponent::SetDirection;
        ut["ClearChanged"] = &DirectionComponent::ClearChanged;
    }

    void DirectionComponent::SetFromLua(DirectionComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            // Enums from Lua can be tricky, typically passed as ints or strings. 
            // Assuming ints for now matching enum values.
            c.currentDirection = static_cast<Direction>(t.get_or("currentDirection", (int)c.currentDirection));
        }
    }

    REGISTER_COMPONENT(DirectionComponent);
}

#include "Components/ColliderComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void ColliderComponent::LoadXml(ColliderComponent& c, tinyxml2::XMLElement* el) {
        if (el->Attribute("width")) c.width = el->FloatAttribute("width");
        if (el->Attribute("height")) c.height = el->FloatAttribute("height");
        if (el->Attribute("offsetX")) c.offsetX = el->FloatAttribute("offsetX");
        if (el->Attribute("offsetY")) c.offsetY = el->FloatAttribute("offsetY");
        if (el->Attribute("isTrigger")) c.isTrigger = el->BoolAttribute("isTrigger");
    }

    void ColliderComponent::Inspect(ColliderComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Width:");
        ImGui::DragFloat("##Width", &c.width, 1.0f, 0.0f, 1000.0f);

        Label("Height:");
        ImGui::DragFloat("##Height", &c.height, 1.0f, 0.0f, 1000.0f);

        Label("Offset X:");
        ImGui::DragFloat("##OffsetX", &c.offsetX, 1.0f, -500.0f, 500.0f);

        Label("Offset Y:");
        ImGui::DragFloat("##OffsetY", &c.offsetY, 1.0f, -500.0f, 500.0f);

        Label("Is Trigger:");
        ImGui::Checkbox("##IsTrigger", &c.isTrigger);
    }

    void ColliderComponent::BindLua(sol::usertype<ColliderComponent>& ut) {
        ut["width"] = &ColliderComponent::width;
        ut["height"] = &ColliderComponent::height;
        ut["offsetX"] = &ColliderComponent::offsetX;
        ut["offsetY"] = &ColliderComponent::offsetY;
        ut["isTrigger"] = &ColliderComponent::isTrigger;
    }

    void ColliderComponent::SetFromLua(ColliderComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            if (t["width"].valid()) c.width = t["width"];
            if (t["height"].valid()) c.height = t["height"];
            if (t["offsetX"].valid()) c.offsetX = t["offsetX"];
            if (t["offsetY"].valid()) c.offsetY = t["offsetY"];
            if (t["isTrigger"].valid()) c.isTrigger = t["isTrigger"];
        }
    }

    REGISTER_COMPONENT(ColliderComponent);
}

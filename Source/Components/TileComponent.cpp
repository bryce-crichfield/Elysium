#include "Components/TileComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void TileComponent::SaveXml(const TileComponent& c, XMLBuilder& builder) {
        builder.AddElement("TileComponent");
    }

    void TileComponent::LoadXml(TileComponent& c, tinyxml2::XMLElement* el) {
        c.tileWidth = el->FloatAttribute("tileWidth", 32.0f);
        c.tileHeight = el->FloatAttribute("tileHeight", 32.0f);
        c.isIsometric = el->BoolAttribute("isIsometric", false);
    }

    void TileComponent::Inspect(TileComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Tile Width:");
        std::string widthId = "##TileWidth_" + std::to_string(e);
        ImGui::DragFloat(widthId.c_str(), &c.tileWidth, 1.0f, 1.0f, 256.0f);

        Label("Tile Height:");
        std::string heightId = "##TileHeight_" + std::to_string(e);
        ImGui::DragFloat(heightId.c_str(), &c.tileHeight, 1.0f, 1.0f, 256.0f);

        Label("Is Isometric:");
        std::string isoId = "##IsIsometric_" + std::to_string(e);
        ImGui::Checkbox(isoId.c_str(), &c.isIsometric);
    }

    void TileComponent::BindLua(sol::usertype<TileComponent>& ut) {
        ut["tileWidth"] = &TileComponent::tileWidth;
        ut["tileHeight"] = &TileComponent::tileHeight;
        ut["isIsometric"] = &TileComponent::isIsometric;
    }

    void TileComponent::SetFromLua(TileComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.tileWidth = t.get_or("tileWidth", c.tileWidth);
            c.tileHeight = t.get_or("tileHeight", c.tileHeight);
            c.isIsometric = t.get_or("isIsometric", c.isIsometric);
        }
    }

    REGISTER_COMPONENT(TileComponent);
}

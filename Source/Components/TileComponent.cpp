#include "Components/TileComponent.h"
#include "Core/Application.h"
#include "Core/ComponentRegistry.h"
#include "Core/Asset.h"
#include "Core/Xml.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "imgui.h"

namespace Elysium {

    void TileComponent::SaveXml(const TileComponent& c, XMLBuilder& builder) {
        if (c.tileName.empty()) return;
        auto element = builder.AddElement("TileComponent")
            .SetAttribute("tileName", c.tileName.c_str())
            .SetAttribute("variantName", c.variantName.c_str())
            .SetAttribute("isIsometric", c.isIsometric);

        bool isWhite = c.tint.r == 255 && c.tint.g == 255 && c.tint.b == 255 && c.tint.a == 255;
        if (!isWhite) {
            std::string tintHex = ColorToHex(c.tint);
            if (!tintHex.empty()) element.SetAttribute("tint", tintHex.c_str());
        }
    }

    void TileComponent::LoadXml(TileComponent& c, tinyxml2::XMLElement* el) {
        const char* tileName = el->Attribute("tileName");
        const char* variantName = el->Attribute("variantName");

        if (tileName) {
            c.tileName = tileName;
            auto& assetService = Application::GetInstance().GetService<Services::AssetService>();
            assetService.LoadAsset(AssetType::TILE, Path(tileName));
        }
        if (variantName) {
            c.variantName = variantName;
        }
        c.isIsometric = el->BoolAttribute("isIsometric", false);
        std::string tintHex = el->Attribute("tint") ? el->Attribute("tint") : "";
        c.tint = ParseHexColor(tintHex, WHITE);

        if (!tileName) {
            LOG_WARNING("Scene", "TileComponent missing tileName attribute");
        }
    }

    void TileComponent::Inspect(TileComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        auto& assetService = Application::GetInstance().GetService<Services::AssetService>();
        const auto& allAssets = assetService.GetAllAssets();

        // Tile asset picker
        Label("Tile: ");
        std::vector<std::string> tileAssetNames;
        tileAssetNames.push_back("<None>");
        for (const auto& [name, asset] : allAssets) {
            if (asset.GetType() == AssetType::TILE && asset.IsLoaded()) {
                tileAssetNames.push_back(name.GetRelativePath());
            }
        }

        std::string currentTileName = c.tileName.empty() ? "<None>" : c.tileName;
        int currentIdx = 0;
        for (size_t i = 0; i < tileAssetNames.size(); ++i) {
            if (tileAssetNames[i] == currentTileName) {
                currentIdx = (int)i;
                break;
            }
        }

        std::string comboId = "##TileAsset_" + std::to_string(e);
        if (ImGui::BeginCombo(comboId.c_str(), currentTileName.c_str())) {
            for (size_t i = 0; i < tileAssetNames.size(); ++i) {
                bool isSelected = (currentIdx == (int)i);
                std::string itemId = tileAssetNames[i] + "##tile" + std::to_string(i);
                if (ImGui::Selectable(itemId.c_str(), isSelected)) {
                    c.tileName = (i == 0) ? "" : tileAssetNames[i];
                    c.variantName = "default";
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Variant picker — populated from the loaded tile asset
        Label("Variant: ");
        Tile tile;
        if (!c.tileName.empty()) {
            tile = assetService.GetTile(Path(c.tileName));
        }

        if (!tile.IsEmpty() && !tile.variants.empty()) {
            std::vector<std::string> variantNames;
            for (const auto& [vname, _] : tile.variants) {
                variantNames.push_back(vname);
            }
            std::sort(variantNames.begin(), variantNames.end());

            int varIdx = 0;
            for (size_t i = 0; i < variantNames.size(); ++i) {
                if (variantNames[i] == c.variantName) { varIdx = (int)i; break; }
            }

            std::string varComboId = "##TileVariant_" + std::to_string(e);
            if (ImGui::BeginCombo(varComboId.c_str(), c.variantName.c_str())) {
                for (size_t i = 0; i < variantNames.size(); ++i) {
                    bool isSelected = (varIdx == (int)i);
                    std::string itemId = variantNames[i] + "##var" + std::to_string(i);
                    if (ImGui::Selectable(itemId.c_str(), isSelected)) {
                        c.variantName = variantNames[i];
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            static char varBuffer[256];
            strncpy(varBuffer, c.variantName.c_str(), sizeof(varBuffer) - 1);
            varBuffer[sizeof(varBuffer) - 1] = '\0';
            std::string inputId = "##TileVariantInput_" + std::to_string(e);
            if (ImGui::InputText(inputId.c_str(), varBuffer, sizeof(varBuffer))) {
                c.variantName = std::string(varBuffer);
            }
        }

        Label("Is Isometric:");
        std::string isoId = "##TileIsIso_" + std::to_string(e);
        ImGui::Checkbox(isoId.c_str(), &c.isIsometric);

        float tint[4] = {c.tint.r / 255.0f, c.tint.g / 255.0f, c.tint.b / 255.0f, c.tint.a / 255.0f};
        Label("Tint: ");
        std::string tintId = "##TileTint_" + std::to_string(e);
        if (ImGui::ColorEdit4(tintId.c_str(), tint)) {
            c.tint = {(unsigned char)(tint[0] * 255), (unsigned char)(tint[1] * 255),
                      (unsigned char)(tint[2] * 255), (unsigned char)(tint[3] * 255)};
        }
    }

    static Color TileObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        if (obj.is<std::string>()) return ParseHexColor(obj.as<std::string>(), WHITE);
        return WHITE;
    }

    void TileComponent::BindLua(sol::usertype<TileComponent>& ut) {
        ut["tileName"]    = &TileComponent::tileName;
        ut["variantName"] = &TileComponent::variantName;
        ut["isIsometric"] = &TileComponent::isIsometric;
        ut["tint"] = sol::property(
            [](TileComponent& c) { return c.tint; },
            [](TileComponent& c, sol::object v) { c.tint = TileObjectToColor(v); });
    }

    void TileComponent::SetFromLua(TileComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.tileName    = t.get_or("tileName",    c.tileName);
            c.variantName = t.get_or("variantName", c.variantName);
            c.isIsometric = t.get_or("isIsometric", c.isIsometric);
            if (t["tint"].valid()) c.tint = TileObjectToColor(t["tint"]);
        }
    }

    REGISTER_COMPONENT(TileComponent);
}

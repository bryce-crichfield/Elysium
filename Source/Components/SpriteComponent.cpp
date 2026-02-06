#include "Components/SpriteComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Application.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "imgui.h"
#include <set>

namespace Elysium {
    SpriteComponent::SpriteComponent(const Sprite& sprite, const std::string& marker)
        : sprite(sprite), markerName(marker) {}

    void SpriteComponent::LoadXml(SpriteComponent& c, tinyxml2::XMLElement* el) {
        const char* spriteName = el->Attribute("spriteName");
        const char* markerName = el->Attribute("markerName");

        if (spriteName && markerName) {
            auto& assetService = Application::GetInstance().GetService<Elysium::Services::AssetService>();
            c.sprite = assetService.GetSprite(spriteName);
            c.markerName = markerName;
        } else {
            LOG_WARNING("Scene", "SpriteComponent missing required attributes: spriteName or markerName");
        }
    }

    void SpriteComponent::Inspect(SpriteComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        // Sprite asset picker
        Label("Sprite Asset: ");
        auto& assetService = Elysium::Application::GetInstance().GetService<Elysium::Services::AssetService>();
        const auto& allAssets = assetService.GetAllAssets();

        // Build list of sprite assets (non-static to avoid conflicts)
        std::vector<std::string> spriteAssetNames;
        spriteAssetNames.push_back("<None>");

        for (const auto& [name, asset] : allAssets) {
            if (asset.GetType() == AssetType::SPRITE && asset.IsLoaded()) {
                spriteAssetNames.push_back(name);
            }
        }

        // Find current selection
        std::string currentSpriteName = c.sprite.name.empty() ? "<None>" : c.sprite.name;
        int currentIndex = 0;
        for (size_t i = 0; i < spriteAssetNames.size(); ++i) {
            if (spriteAssetNames[i] == currentSpriteName) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        std::string assetComboId = "##SpriteAsset_" + std::to_string(e);
        if (ImGui::BeginCombo(assetComboId.c_str(), currentSpriteName.c_str())) {
            for (size_t i = 0; i < spriteAssetNames.size(); ++i) {
                bool isSelected = (currentIndex == static_cast<int>(i));
                std::string selectableId = spriteAssetNames[i] + "##" + std::to_string(i);
                if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                    if (i == 0) {
                        // Clear sprite
                        c.sprite = Sprite();
                    } else {
                        // Assign sprite from asset service
                        c.sprite = assetService.GetSprite(spriteAssetNames[i]);
                    }
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Marker picker (only if sprite is loaded)
        Label("Marker: ");
        if (!c.sprite.name.empty() && !c.sprite.sheets.empty()) {
            // Collect unique markers from all sheets (deduplicated)
            std::vector<std::string> markerNames;
            markerNames.push_back("<None>");

            std::set<std::string> uniqueMarkers;
            for (const auto& [sheetName, sheet] : c.sprite.sheets) {
                for (const auto& [markerName, marker] : sheet.markers) {
                    uniqueMarkers.insert(markerName);
                }
            }

            for (const auto& markerName : uniqueMarkers) {
                markerNames.push_back(markerName);
            }

            // Find current selection
            std::string currentMarker = c.markerName.empty() ? "<None>" : c.markerName;
            int markerIndex = 0;
            for (size_t i = 0; i < markerNames.size(); ++i) {
                if (markerNames[i] == currentMarker) {
                    markerIndex = static_cast<int>(i);
                    break;
                }
            }

            std::string comboId = "##SpriteMarker_" + std::to_string(e);
            if (ImGui::BeginCombo(comboId.c_str(), currentMarker.c_str())) {
                for (size_t i = 0; i < markerNames.size(); ++i) {
                    bool isSelected = (markerIndex == static_cast<int>(i));
                    std::string selectableId = markerNames[i] + "##" + std::to_string(i);
                    if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                        c.markerName = (i == 0) ? "" : markerNames[i];
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            // Fallback to text input if no sprite loaded
            static char markerBuffer[256];
            strncpy(markerBuffer, c.markerName.c_str(), sizeof(markerBuffer) - 1);
            markerBuffer[sizeof(markerBuffer) - 1] = '\0';

            std::string inputId = "##SpriteMarkerInput_" + std::to_string(e);
            if (ImGui::InputText(inputId.c_str(), markerBuffer, sizeof(markerBuffer))) {
                c.markerName = std::string(markerBuffer);
            }
        }

        Label("Frame Index: ");
        std::string frameIndexId = "##SpriteFrameIndex_" + std::to_string(e);
        ImGui::DragInt(frameIndexId.c_str(), &c.frameIndex, 1.0f, 0);

        Label("Duration: ");
        std::string durationId = "##SpriteFrameDuration_" + std::to_string(e);
        ImGui::DragFloat(durationId.c_str(), &c.frameDuration, 0.01f, 0.0f, 10.0f);

        Label("Elapsed: ");
        std::string elapsedId = "##SpriteFrameElapsed_" + std::to_string(e);
        ImGui::DragFloat(elapsedId.c_str(), &c.frameElapsed, 0.01f, 0.0f);
    }

    void SpriteComponent::BindLua(sol::usertype<SpriteComponent>& ut) {
        ut["marker"] = &SpriteComponent::markerName;
    }

    void SpriteComponent::SetFromLua(SpriteComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.markerName = t.get_or("marker", c.markerName);
        }
    }

    REGISTER_COMPONENT(SpriteComponent);
}

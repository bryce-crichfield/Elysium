#include "Components/SpriteComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Application.h"
#include "Core/Xml.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "imgui.h"
#include <set>

namespace Elysium {
    SpriteComponent::SpriteComponent(const Sprite& sprite, const std::string& sequence)
        : spriteName(sprite.name), sequenceName(sequence) {}

    void SpriteComponent::SaveXml(const SpriteComponent& c, XMLBuilder& builder) {
        if (c.spriteName.empty()) return;
        builder.AddElement("SpriteComponent")
            .SetAttribute("spriteName", c.spriteName.c_str())
            .SetAttribute("sheetName", c.sheetName.c_str())
            .SetAttribute("sequenceName", c.sequenceName.c_str());
    }

    void SpriteComponent::LoadXml(SpriteComponent& c, tinyxml2::XMLElement* el) {
        const char* spriteName = el->Attribute("spriteName");
        const char* sheetName = el->Attribute("sheetName");
        const char* sequenceName = el->Attribute("sequenceName");

        if (spriteName) {
            c.spriteName = spriteName;

            // Load the sprite if not already loaded
            auto& assetService = Elysium::Application::GetInstance().GetService<Elysium::Services::AssetService>();
            assetService.LoadAsset(AssetType::SPRITE, Path(spriteName));
        }
        if (sheetName) {
            c.sheetName = sheetName;
        }
        if (sequenceName) {
            c.sequenceName = sequenceName;
        }

        if (!spriteName || !sheetName || !sequenceName) {
            LOG_WARNING("Scene", "SpriteComponent missing attributes: spriteName, sheetName, or sequenceName");
        }
    }

    void SpriteComponent::Inspect(SpriteComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        auto& assetService = Elysium::Application::GetInstance().GetService<Elysium::Services::AssetService>();
        const auto& allAssets = assetService.GetAllAssets();

        // Sprite asset picker
        Label("Sprite: ");
        std::vector<std::string> spriteAssetNames;
        spriteAssetNames.push_back("<None>");

        for (const auto& [name, asset] : allAssets) {
            if (asset->GetType() == AssetType::SPRITE && asset->IsLoaded()) {
                spriteAssetNames.push_back(name.GetRelativePath());
            }
        }

        std::string currentSpriteName = c.spriteName.empty() ? "<None>" : c.spriteName;
        int currentSpriteIdx = 0;
        for (size_t i = 0; i < spriteAssetNames.size(); ++i) {
            if (spriteAssetNames[i] == currentSpriteName) {
                currentSpriteIdx = static_cast<int>(i);
                break;
            }
        }

        std::string spriteComboId = "##SpriteAsset_" + std::to_string(e);
        if (ImGui::BeginCombo(spriteComboId.c_str(), currentSpriteName.c_str())) {
            for (size_t i = 0; i < spriteAssetNames.size(); ++i) {
                bool isSelected = (currentSpriteIdx == static_cast<int>(i));
                std::string selectableId = spriteAssetNames[i] + "##" + std::to_string(i);
                if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                    c.spriteName = (i == 0) ? "" : spriteAssetNames[i];
                    c.sheetName = "";
                    c.sequenceName = "";
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Get sprite for sheet/sequence pickers
        Sprite sprite;
        if (!c.spriteName.empty()) {
            sprite = assetService.GetSprite(Path(c.spriteName));
        }

        // Sheet picker
        Label("Sheet: ");
        if (!sprite.name.empty() && !sprite.sheets.empty()) {
            std::vector<std::string> sheetNames;
            sheetNames.push_back("<None>");
            for (const auto& [sheetName, sheet] : sprite.sheets) {
                sheetNames.push_back(sheetName);
            }

            std::string currentSheet = c.sheetName.empty() ? "<None>" : c.sheetName;
            int sheetIdx = 0;
            for (size_t i = 0; i < sheetNames.size(); ++i) {
                if (sheetNames[i] == currentSheet) {
                    sheetIdx = static_cast<int>(i);
                    break;
                }
            }

            std::string sheetComboId = "##SpriteSheet_" + std::to_string(e);
            if (ImGui::BeginCombo(sheetComboId.c_str(), currentSheet.c_str())) {
                for (size_t i = 0; i < sheetNames.size(); ++i) {
                    bool isSelected = (sheetIdx == static_cast<int>(i));
                    std::string selectableId = sheetNames[i] + "##sheet" + std::to_string(i);
                    if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                        c.sheetName = (i == 0) ? "" : sheetNames[i];
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            static char sheetBuffer[256];
            strncpy(sheetBuffer, c.sheetName.c_str(), sizeof(sheetBuffer) - 1);
            sheetBuffer[sizeof(sheetBuffer) - 1] = '\0';
            std::string inputId = "##SpriteSheetInput_" + std::to_string(e);
            if (ImGui::InputText(inputId.c_str(), sheetBuffer, sizeof(sheetBuffer))) {
                c.sheetName = std::string(sheetBuffer);
            }
        }

        // Sequence picker
        Label("Sequence: ");
        if (!sprite.name.empty() && !c.sheetName.empty() && sprite.sheets.count(c.sheetName)) {
            const auto& sheet = sprite.sheets.at(c.sheetName);
            std::vector<std::string> sequenceNames;
            sequenceNames.push_back("<None>");
            for (const auto& [seqName, seq] : sheet.sequences) {
                sequenceNames.push_back(seqName);
            }

            std::string currentSequence = c.sequenceName.empty() ? "<None>" : c.sequenceName;
            int sequenceIdx = 0;
            for (size_t i = 0; i < sequenceNames.size(); ++i) {
                if (sequenceNames[i] == currentSequence) {
                    sequenceIdx = static_cast<int>(i);
                    break;
                }
            }

            std::string seqComboId = "##SpriteSequence_" + std::to_string(e);
            if (ImGui::BeginCombo(seqComboId.c_str(), currentSequence.c_str())) {
                for (size_t i = 0; i < sequenceNames.size(); ++i) {
                    bool isSelected = (sequenceIdx == static_cast<int>(i));
                    std::string selectableId = sequenceNames[i] + "##seq" + std::to_string(i);
                    if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                        c.sequenceName = (i == 0) ? "" : sequenceNames[i];
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            static char sequenceBuffer[256];
            strncpy(sequenceBuffer, c.sequenceName.c_str(), sizeof(sequenceBuffer) - 1);
            sequenceBuffer[sizeof(sequenceBuffer) - 1] = '\0';
            std::string inputId = "##SpriteSequenceInput_" + std::to_string(e);
            if (ImGui::InputText(inputId.c_str(), sequenceBuffer, sizeof(sequenceBuffer))) {
                c.sequenceName = std::string(sequenceBuffer);
            }
        }

        Label("Sequence Index: ");
        std::string seqIndexId = "##SpriteSequenceIndex_" + std::to_string(e);
        ImGui::DragInt(seqIndexId.c_str(), &c.sequenceIndex, 1.0f, 0);

        Label("Duration: ");
        std::string durationId = "##SpriteFrameDuration_" + std::to_string(e);
        ImGui::DragFloat(durationId.c_str(), &c.frameDuration, 0.01f, 0.0f, 10.0f);

        Label("Elapsed: ");
        std::string elapsedId = "##SpriteFrameElapsed_" + std::to_string(e);
        ImGui::DragFloat(elapsedId.c_str(), &c.frameElapsed, 0.01f, 0.0f);
    }

    void SpriteComponent::BindLua(sol::usertype<SpriteComponent>& ut) {
        ut["sprite"] = &SpriteComponent::spriteName;
        ut["sheet"] = &SpriteComponent::sheetName;
        ut["sequence"] = &SpriteComponent::sequenceName;
        ut["duration"] = &SpriteComponent::frameDuration;
    }

    void SpriteComponent::SetFromLua(SpriteComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.spriteName = t.get_or("sprite", c.spriteName);
            c.sheetName = t.get_or("sheet", c.sheetName);
            c.sequenceName = t.get_or("sequence", c.sequenceName);
        }
    }

    REGISTER_COMPONENT(SpriteComponent);
}

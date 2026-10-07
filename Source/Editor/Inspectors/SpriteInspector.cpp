#include "Core/Components/SpriteComponent.h"

#include <cstring>
#include "Core/Path.h"
#include "Core/Sprite.h"
#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Widgets/AssetField.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "imgui.h"

namespace Elysium {

void InspectSprite(SpriteComponent& c, Entity e, ServiceLocator& services) {
    auto& assetService = services.Get<Elysium::Services::IAssetService>();

    if (AssetFieldRow("Sprite", AssetKind::Sprite, c.spriteName)) {
        c.sheetName.clear();
        c.sequenceName.clear();
        if (!c.spriteName.empty()) assetService.LoadAsset<Sprite>(Path(c.spriteName));
    }

    // Get sprite for sheet/sequence pickers
    Sprite sprite;
    if (!c.spriteName.empty()) {
        if (auto* spriteData = assetService.Get<Sprite>(Path(c.spriteName))) {
            sprite = *spriteData;
        }
    }

    // Sheet picker
    PropertyLabel("Sheet");
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
    PropertyLabel("Sequence");
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

    PropertyLabel("Sequence Index");
    std::string seqIndexId = "##SpriteSequenceIndex_" + std::to_string(e);
    ImGui::DragInt(seqIndexId.c_str(), &c.sequenceIndex, 1.0f, 0);

    PropertyLabel("Duration");
    std::string durationId = "##SpriteFrameDuration_" + std::to_string(e);
    ImGui::DragFloat(durationId.c_str(), &c.frameDuration, 0.01f, 0.0f, 10.0f);

    PropertyLabel("Elapsed");
    std::string elapsedId = "##SpriteFrameElapsed_" + std::to_string(e);
    ImGui::DragFloat(elapsedId.c_str(), &c.frameElapsed, 0.01f, 0.0f);
}

}  // namespace Elysium

#include "Core/Components/ScriptComponent.h"

#include "Core/Path.h"
#include "Core/Script.h"
#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Widgets/AssetField.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IScriptService.h"
#include "imgui.h"

namespace Elysium {

void InspectScript(ScriptComponent& c, Entity e, ServiceLocator& services) {
    auto& assetService = services.Get<Elysium::Services::IAssetService>();

    PropertyLabel("Is Active");
    std::string activeId = "##ScriptActive_" + std::to_string(e);
    ImGui::Checkbox(activeId.c_str(), &c.isActive);

    SectionHeader("Scripts");

    int removeIndex = -1;
    for (size_t i = 0; i < c.scriptNames.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        ImGui::SetNextItemWidth(-(ButtonWidth(ICON_FA_XMARK) + ImGui::GetStyle().ItemSpacing.x));
        if (AssetField("##Script", AssetKind::Script, c.scriptNames[i])) {
            c.isInitialized[i] = false;
            if (!c.scriptNames[i].empty()) assetService.LoadAsset<Script>(Path(c.scriptNames[i]));
        }

        ImGui::SameLine();
        if (IconButton(ICON_FA_XMARK, "Remove script")) {
            removeIndex = static_cast<int>(i);
        }

        ImGui::PopID();
    }

    if (removeIndex >= 0) {
        c.RemoveScript(static_cast<size_t>(removeIndex));
    }

    if (ImGui::Button(ICON_FA_PLUS "  Add Script", ImVec2(-FLT_MIN, 0))) {
        c.AddScript("");
    }

    // Show script data for all active scripts
    if (c.isActive) {
        auto& scriptService = services.Get<Elysium::Services::IScriptService>();
        for (size_t i = 0; i < c.scriptNames.size(); ++i) {
            if (!c.scriptNames[i].empty()) {
                std::string header = ICON_FA_CODE "  " + c.scriptNames[i];
                if (ImGui::CollapsingHeader(header.c_str())) {
                    scriptService.InspectEntityScript(e, Path(c.scriptNames[i]));
                }
            }
        }
    }
}

}  // namespace Elysium

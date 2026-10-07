#include "Core/Components/MaterialComponent.h"

#include <algorithm>
#include <filesystem>
#include "Core/Assets/ShaderAsset.h"
#include "Core/Graphics.h"
#include "Core/Path.h"
#include "Core/Shader.h"
#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Widgets/AssetField.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "imgui.h"

namespace Elysium {

namespace {
    // Every chunk in Assets/Shaders/Sdf/Material, by stem. Listed once: new chunks
    // need a restart to show up in the inspector.
    const std::vector<std::string>& AvailableMaterials() {
        static const std::vector<std::string> materials = [] {
            std::vector<std::string> names;
            std::error_code error;
            const std::string dir = Path("Shaders/Sdf/Material", PathRoot::Engine).GetFullPath();
            for (const auto& entry : std::filesystem::directory_iterator(dir, error)) {
                if (entry.path().extension() == ".glsl") names.push_back(entry.path().stem().string());
            }
            std::sort(names.begin(), names.end());
            return names;
        }();
        return materials;
    }

    // Material uniforms don't depend on the geometry half of the composition, so the
    // inspector reflects them off the Rect variant regardless of the entity's shape.
    constexpr const char* kInspectGeometry = "Rect";
}

void InspectMaterial(MaterialComponent& c, Entity e, ServiceLocator& services) {
    auto& assetService = services.Get<Services::IAssetService>();

    PropertyLabel("Enabled");
    std::string enabledId = "##MaterialEnabled_" + std::to_string(e);
    ImGui::Checkbox(enabledId.c_str(), &c.enabled);

    PropertyLabel("Padding");
    std::string paddingId = "##MaterialPadding_" + std::to_string(e);
    ImGui::DragFloat(paddingId.c_str(), &c.padding, 1.0f, 0.0f, 512.0f);

    int removeIndex = -1;
    int moveUpIndex = -1;
    for (size_t i = 0; i < c.layers.size(); ++i) {
        MaterialLayer& layer = c.layers[i];
        ImGui::PushID((int)i);

        std::string header = std::to_string(i) + ": " + layer.material;
        bool open = ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        AlignRight(ButtonWidth(ICON_FA_ARROW_UP) + ButtonWidth(ICON_FA_XMARK) + ImGui::GetStyle().ItemSpacing.x);
        ImGui::BeginDisabled(i == 0);
        if (IconButton(ICON_FA_ARROW_UP, "Move up")) moveUpIndex = (int)i;
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (IconButton(ICON_FA_XMARK, "Remove layer")) removeIndex = (int)i;

        if (open) {
            PropertyLabel("Enabled");
            ImGui::Checkbox("##LayerEnabled", &layer.enabled);

            PropertyLabel("Material");
            if (ImGui::BeginCombo("##LayerMaterial", layer.material.c_str())) {
                for (const std::string& material : AvailableMaterials()) {
                    bool isSelected = layer.material == material;
                    if (ImGui::Selectable(material.c_str(), isSelected) && !isSelected) {
                        layer.material = material;
                        layer.overrides.clear();
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (layer.material == "Texture") {
                if (AssetFieldRow("Texture", AssetKind::Texture, layer.texturePath) && !layer.texturePath.empty()) {
                    assetService.LoadAsset<Texture>(Path(layer.texturePath));
                }
            }

            Path shaderPath = ComposedShaderPath(kInspectGeometry, layer.material);
            if (Shader* shader = assetService.Get<Shader>(shaderPath); shader && shader->IsValid()) {
                InspectUniformOverrides(*shader, layer.overrides);
            } else if (!assetService.IsAssetLoaded(shaderPath)) {
                assetService.LoadAsset<Shader>(shaderPath);
                ImGui::TextDisabled("Loading shader...");
            }
        }

        ImGui::PopID();
    }

    if (removeIndex >= 0) c.layers.erase(c.layers.begin() + removeIndex);
    if (moveUpIndex > 0) std::swap(c.layers[moveUpIndex], c.layers[moveUpIndex - 1]);

    if (ImGui::Button(ICON_FA_PLUS "  Add Layer", ImVec2(-FLT_MIN, 0))) c.layers.push_back(MaterialLayer{});
}

}  // namespace Elysium

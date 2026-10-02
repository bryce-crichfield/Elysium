#include "Core/Components/ShaderComponent.h"

#include <cctype>
#include "Core/Assets/ShaderAsset.h"
#include "Core/Path.h"
#include "Core/Shader.h"
#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Widgets/AssetField.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "imgui.h"

namespace Elysium {

void InspectUniformOverrides(const Shader& shader, std::unordered_map<std::string, Value>& overrides) {
    auto IsColor = [](std::string name) {
        for (char& ch : name) ch = (char)tolower((unsigned char)ch);
        return name.find("color") != std::string::npos;
    };

    for (const ShaderUniform& uniform : shader.GetUniforms()) {
        if (uniform.isBuiltIn) continue;
        // Shows the value in effect (override or source default); editing creates the
        // override, Reset (or editing back to the default) drops it.
        auto it = overrides.find(uniform.name);
        bool hasOverride = it != overrides.end();
        Value value = hasOverride ? it->second : uniform.defaultValue;
        if (!InspectValueRow(uniform.name, value, uniform.defaultValue, !hasOverride, IsColor(uniform.name))) continue;
        if (value == uniform.defaultValue) overrides.erase(uniform.name);
        else overrides[uniform.name] = value;
    }
}

void InspectShader(ShaderComponent& c, Entity e, ServiceLocator& services) {
    auto& assetService = services.Get<Services::IAssetService>();

    PropertyLabel("Enabled");
    std::string enabledId = "##ShaderEnabled_" + std::to_string(e);
    ImGui::Checkbox(enabledId.c_str(), &c.enabled);

    // Composed .sdf shaders belong to MaterialComponent, not the silhouette path.
    const auto silhouette = [](const std::string& path) { return !IsComposedShaderPath(Path(path)); };
    if (AssetFieldRow("Shader", AssetKind::Shader, c.shaderPath, silhouette)) {
        c.overrides.clear();
        if (!c.shaderPath.empty()) assetService.LoadAsset<Shader>(Path(c.shaderPath));
    }

    PropertyLabel("Padding");
    std::string paddingId = "##ShaderPadding_" + std::to_string(e);
    ImGui::DragFloat(paddingId.c_str(), &c.padding, 1.0f, 0.0f, 512.0f);

    PropertyLabel("Size");
    std::string sizeId = "##ShaderSize_" + std::to_string(e);
    float size[2] = {c.width, c.height};
    if (ImGui::DragFloat2(sizeId.c_str(), size, 1.0f, 1.0f, 4096.0f)) {
        c.width = size[0];
        c.height = size[1];
    }

    if (c.shaderPath.empty()) return;
    auto* shader = assetService.Get<Shader>(Path(c.shaderPath));
    if (!shader || !shader->IsValid()) return;

    SectionHeader("Uniforms");
    InspectUniformOverrides(*shader, c.overrides);
}

}  // namespace Elysium

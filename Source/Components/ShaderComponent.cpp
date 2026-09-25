#include "Components/ShaderComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "Core/Editor.h"
#include "Core/Shader.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"
#include "Core/Assets/ShaderAsset.h"
#include "imgui.h"
#include <cctype>

namespace Elysium {
    void ShaderComponent::SaveXml(const ShaderComponent& c, XMLBuilder& builder) {
        auto el = builder.AddElement("ShaderComponent")
            .SetAttribute("shader", c.shaderPath.c_str())
            .SetAttribute("padding", c.padding)
            .SetAttribute("enabled", c.enabled)
            .SetAttribute("width", c.width)
            .SetAttribute("height", c.height);
        SaveUniformOverrides(el, c.overrides);
    }

    void ShaderComponent::LoadXml(ShaderComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.shaderPath = el->Attribute("shader") ? el->Attribute("shader") : "";
        c.padding = el->FloatAttribute("padding", 0.0f);
        c.enabled = el->BoolAttribute("enabled", true);
        c.width = el->FloatAttribute("width", 100.0f);
        c.height = el->FloatAttribute("height", 100.0f);

        if (!c.shaderPath.empty()) {
            auto& assetService = services.Get<Elysium::Services::IAssetService>();
            assetService.LoadAsset<Shader>(Path(c.shaderPath));
        }

        LoadUniformOverrides(el, c.overrides);
    }

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

    void LoadUniformOverrides(tinyxml2::XMLElement* el, std::unordered_map<std::string, Value>& overrides) {
        for (auto* child = el->FirstChildElement("Uniform"); child; child = child->NextSiblingElement("Uniform")) {
            const char* name = child->Attribute("name");
            const char* type = child->Attribute("type");
            const char* value = child->Attribute("value");
            if (!name || !type || !value) continue;
            overrides[name] = Value::FromString(type, value);
        }
    }

    void SaveUniformOverrides(XMLBuilder& el, const std::unordered_map<std::string, Value>& overrides) {
        for (const auto& [name, value] : overrides) {
            el.AddElement("Uniform")
                .SetAttribute("name", name.c_str())
                .SetAttribute("type", value.TypeName().c_str())
                .SetAttribute("value", value.ToString().c_str());
        }
    }

    void ShaderComponent::Inspect(ShaderComponent& c, Entity e, ServiceLocator& services) {

        auto& assetService = services.Get<Services::IAssetService>();

        PropertyLabel("Enabled");
        std::string enabledId = "##ShaderEnabled_" + std::to_string(e);
        ImGui::Checkbox(enabledId.c_str(), &c.enabled);

        std::vector<std::string> shaderPaths;
        for (const auto& [path, asset] : assetService.GetAllAssets()) {
            // Composed .sdf shaders belong to MaterialComponent, not the silhouette path.
            if (!IsComposedShaderPath(path) && asset->IsLoaded() && assetService.GetData<Shader>(asset.get())) {
                shaderPaths.push_back(path.GetRelativePath());
            }
        }

        PropertyLabel("Shader");
        std::string shaderId = "##ShaderPath_" + std::to_string(e);
        if (InspectPathCombo(shaderId.c_str(), c.shaderPath, shaderPaths)) {
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

    void ShaderComponent::BindLua(sol::usertype<ShaderComponent>& ut) {
        ut["shaderPath"] = &ShaderComponent::shaderPath;
        ut["padding"] = &ShaderComponent::padding;
        ut["enabled"] = &ShaderComponent::enabled;
    }

    REGISTER_COMPONENT(ShaderComponent);
}

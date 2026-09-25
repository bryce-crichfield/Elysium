#include "Components/ShaderComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Shader.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"
#include "Services/AssetService.h"
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
        for (const auto& [name, value] : c.overrides) {
            el.AddElement("Uniform")
                .SetAttribute("name", name.c_str())
                .SetAttribute("type", value.TypeName().c_str())
                .SetAttribute("value", value.ToString().c_str());
        }
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

        for (auto* child = el->FirstChildElement("Uniform"); child; child = child->NextSiblingElement("Uniform")) {
            const char* name = child->Attribute("name");
            const char* type = child->Attribute("type");
            const char* value = child->Attribute("value");
            if (!name || !type || !value) continue;
            c.overrides[name] = Value::FromString(type, value);
        }
    }

    void InspectUniformOverrides(const Shader& shader, std::unordered_map<std::string, Value>& overrides) {
        auto ContainsIgnoreCase = [](const std::string& haystack, const char* needle) {
            std::string lower = haystack;
            for (char& ch : lower) ch = (char)tolower((unsigned char)ch);
            return lower.find(needle) != std::string::npos;
        };

        for (const ShaderUniform& uniform : shader.GetUniforms()) {
            if (uniform.isBuiltIn) continue;

            ImGui::PushID(uniform.name.c_str());

            // Always shows the value that's actually in effect (an existing override, or
            // the shader source's default) and always editable — dragging it creates the
            // override; Reset removes it again. No hidden gate to trip over.
            auto it = overrides.find(uniform.name);
            bool hasOverride = it != overrides.end();
            Value value = hasOverride ? it->second : uniform.defaultValue;
            bool changed = false;

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(hasOverride ? ImVec4(1, 1, 1, 1) : ImVec4(0.6f, 0.6f, 0.6f, 1),
                                "%s", uniform.name.c_str());
            ImGui::SameLine(160.0f);
            ImGui::SetNextItemWidth(-56);

            std::string widgetId = "##Value_" + uniform.name;
            if (value.Is<float>()) {
                float v = value.As<float>();
                if (ImGui::DragFloat(widgetId.c_str(), &v, 0.05f)) { value = Value(v); changed = true; }
            } else if (value.Is<int>()) {
                int v = value.As<int>();
                if (ImGui::DragInt(widgetId.c_str(), &v)) { value = Value(v); changed = true; }
            } else if (value.Is<bool>()) {
                bool v = value.As<bool>();
                if (ImGui::Checkbox(widgetId.c_str(), &v)) { value = Value(v); changed = true; }
            } else if (value.Is<Vector2>()) {
                Vector2 v = value.As<Vector2>();
                float f[2] = {v.x, v.y};
                if (ImGui::DragFloat2(widgetId.c_str(), f, 0.05f)) { value = Value(Vector2{f[0], f[1]}); changed = true; }
            } else if (value.Is<Vector3>()) {
                Vector3 v = value.As<Vector3>();
                float f[3] = {v.x, v.y, v.z};
                bool isColor = ContainsIgnoreCase(uniform.name, "color");
                bool edited = isColor ? ImGui::ColorEdit3(widgetId.c_str(), f)
                                      : ImGui::DragFloat3(widgetId.c_str(), f, 0.05f);
                if (edited) { value = Value(Vector3{f[0], f[1], f[2]}); changed = true; }
            } else if (value.Is<Vector4>()) {
                Vector4 v = value.As<Vector4>();
                float f[4] = {v.x, v.y, v.z, v.w};
                bool isColor = ContainsIgnoreCase(uniform.name, "color");
                bool edited = isColor ? ImGui::ColorEdit4(widgetId.c_str(), f)
                                      : ImGui::DragFloat4(widgetId.c_str(), f, 0.05f);
                if (edited) { value = Value(Vector4{f[0], f[1], f[2], f[3]}); changed = true; }
            }

            ImGui::SameLine();
            ImGui::BeginDisabled(!hasOverride);
            if (ImGui::SmallButton("Reset")) { overrides.erase(uniform.name); changed = false; }
            ImGui::EndDisabled();

            if (changed) overrides[uniform.name] = value;

            ImGui::PopID();
        }
    }

    void ShaderComponent::Inspect(ShaderComponent& c, Entity e, ServiceLocator& services) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        auto& assetService = services.Get<Services::IAssetService>();

        Label("Enabled: ");
        std::string enabledId = "##ShaderEnabled_" + std::to_string(e);
        ImGui::Checkbox(enabledId.c_str(), &c.enabled);

        std::vector<std::string> shaderPaths;
        shaderPaths.push_back("<None>");
        for (const auto& [path, asset] : assetService.GetAllAssets()) {
            // Composed .sdf shaders belong to MaterialComponent, not the silhouette path.
            const std::string& relative = path.GetRelativePath();
            bool isComposed = relative.size() >= 4 && relative.compare(relative.size() - 4, 4, ".sdf") == 0;
            if (!isComposed && asset->IsLoaded() && assetService.GetData<Shader>(asset.get())) {
                shaderPaths.push_back(path.GetRelativePath());
            }
        }

        Label("Shader: ");
        std::string currentShader = c.shaderPath.empty() ? "<None>" : c.shaderPath;
        std::string shaderId = "##ShaderPath_" + std::to_string(e);
        if (ImGui::BeginCombo(shaderId.c_str(), currentShader.c_str())) {
            for (size_t i = 0; i < shaderPaths.size(); ++i) {
                bool isSelected = (shaderPaths[i] == currentShader);
                std::string selectableId = shaderPaths[i] + "##" + std::to_string(i);
                if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                    c.shaderPath = (i == 0) ? "" : shaderPaths[i];
                    c.overrides.clear();
                    if (!c.shaderPath.empty()) assetService.LoadAsset<Shader>(Path(c.shaderPath));
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        Label("Padding: ");
        std::string paddingId = "##ShaderPadding_" + std::to_string(e);
        ImGui::DragFloat(paddingId.c_str(), &c.padding, 1.0f, 0.0f, 512.0f);

        Label("Size: ");
        std::string sizeId = "##ShaderSize_" + std::to_string(e);
        float size[2] = {c.width, c.height};
        if (ImGui::DragFloat2(sizeId.c_str(), size, 1.0f, 1.0f, 4096.0f)) {
            c.width = size[0];
            c.height = size[1];
        }

        if (c.shaderPath.empty()) return;
        auto* shader = assetService.Get<Shader>(Path(c.shaderPath));
        if (!shader || !shader->IsValid()) return;

        ImGui::Spacing();
        ImGui::TextDisabled("Uniforms");
        InspectUniformOverrides(*shader, c.overrides);
    }

    void ShaderComponent::BindLua(sol::usertype<ShaderComponent>& ut) {
        ut["shaderPath"] = &ShaderComponent::shaderPath;
        ut["padding"] = &ShaderComponent::padding;
        ut["enabled"] = &ShaderComponent::enabled;
    }

    REGISTER_COMPONENT(ShaderComponent);
}

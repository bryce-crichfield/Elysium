#include "Core/Components/ShaderComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Shader.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"
#include "Core/Assets/ShaderAsset.h"
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

    FieldList ShaderComponent::Fields() {
        return {
            Field("Enabled", &ShaderComponent::enabled, "enabled"),
            Field("Shader", &ShaderComponent::shaderPath, "shader").Asset(AssetKind::Shader),
            Field("Padding", &ShaderComponent::padding, "padding"),
            Field("Width", &ShaderComponent::width, "width"),
            Field("Height", &ShaderComponent::height, "height"),
        };
    }

    void ShaderComponent::BindLua(sol::usertype<ShaderComponent>& ut) {
        ut["shaderPath"] = &ShaderComponent::shaderPath;
        ut["padding"] = &ShaderComponent::padding;
        ut["enabled"] = &ShaderComponent::enabled;
    }

    REGISTER_COMPONENT(ShaderComponent);
}

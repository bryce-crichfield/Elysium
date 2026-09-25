#include "Components/MaterialComponent.h"
#include "Components/ShaderComponent.h"
#include "Core/Assets/ShaderAsset.h"
#include "Core/ComponentRegistry.h"
#include "Core/Editor.h"
#include "Core/Graphics.h"
#include "Core/Shader.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"
#include "imgui.h"

#include <algorithm>
#include <filesystem>
#include <optional>

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

    void MaterialComponent::SaveXml(const MaterialComponent& c, XMLBuilder& builder) {
        auto el = builder.AddElement("MaterialComponent")
            .SetAttribute("padding", c.padding)
            .SetAttribute("enabled", c.enabled);
        for (const MaterialLayer& layer : c.layers) {
            auto layerEl = el.AddElement("Layer")
                .SetAttribute("material", layer.material.c_str())
                .SetAttribute("enabled", layer.enabled);
            if (!layer.texturePath.empty()) layerEl.SetAttribute("texture", layer.texturePath.c_str());
            SaveUniformOverrides(layerEl, layer.overrides);
        }
    }

    void MaterialComponent::LoadXml(MaterialComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        auto& assetService = services.Get<Elysium::Services::IAssetService>();

        c.padding = el->FloatAttribute("padding", 0.0f);
        c.enabled = el->BoolAttribute("enabled", true);
        c.layers.clear();

        for (auto* layerEl = el->FirstChildElement("Layer"); layerEl; layerEl = layerEl->NextSiblingElement("Layer")) {
            MaterialLayer layer;
            if (const char* material = layerEl->Attribute("material")) layer.material = material;
            layer.enabled = layerEl->BoolAttribute("enabled", true);
            if (const char* texture = layerEl->Attribute("texture")) layer.texturePath = texture;
            if (!layer.texturePath.empty()) assetService.LoadAsset<Texture>(Path(layer.texturePath));
            LoadUniformOverrides(layerEl, layer.overrides);
            c.layers.push_back(std::move(layer));
        }
    }

    void MaterialComponent::Inspect(MaterialComponent& c, Entity e, ServiceLocator& services) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        auto& assetService = services.Get<Services::IAssetService>();

        Label("Enabled: ");
        std::string enabledId = "##MaterialEnabled_" + std::to_string(e);
        ImGui::Checkbox(enabledId.c_str(), &c.enabled);

        Label("Padding: ");
        std::string paddingId = "##MaterialPadding_" + std::to_string(e);
        ImGui::DragFloat(paddingId.c_str(), &c.padding, 1.0f, 0.0f, 512.0f);

        std::vector<std::string> texturePaths;
        for (const auto& [path, asset] : assetService.GetAllAssets()) {
            if (asset->IsLoaded() && assetService.GetData<Texture>(asset.get())) {
                texturePaths.push_back(path.GetRelativePath());
            }
        }

        int removeIndex = -1;
        int moveUpIndex = -1;
        for (size_t i = 0; i < c.layers.size(); ++i) {
            MaterialLayer& layer = c.layers[i];
            ImGui::PushID((int)i);

            std::string header = std::to_string(i) + ": " + layer.material;
            bool open = ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
            ImGui::BeginDisabled(i == 0);
            if (ImGui::SmallButton("Up")) moveUpIndex = (int)i;
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) removeIndex = (int)i;

            if (open) {
                Label("Enabled: ");
                ImGui::Checkbox("##LayerEnabled", &layer.enabled);

                Label("Material: ");
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
                    Label("Texture: ");
                    InspectPathCombo("##LayerTexture", layer.texturePath, texturePaths);
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

        if (ImGui::Button("Add Layer")) c.layers.push_back(MaterialLayer{});
    }

    namespace {
        // Lua -> uniform value. Colors (a Color, a "#RRGGBBAA" string, or an {r,g,b,a}
        // table in 0..255) become vec4 in 0..1; {x,y[,z[,w]]} tables become vecN.
        std::optional<Value> ValueFromLua(const sol::object& v) {
            if (v.is<bool>()) return Value{v.as<bool>()};
            if (v.is<double>()) return Value{(float)v.as<double>()};
            if (v.is<Color>()) return Value{v.as<Color>()};
            if (v.is<std::string>()) return Value{ParseHexColor(v.as<std::string>(), Colors::White)};
            if (v.is<Vector2>()) return Value{v.as<Vector2>()};
            if (!v.is<sol::table>()) return std::nullopt;

            sol::table t = v.as<sol::table>();
            if (t["r"].valid()) {
                return Value{Color{(unsigned char)t.get_or("r", 255), (unsigned char)t.get_or("g", 255),
                                   (unsigned char)t.get_or("b", 255), (unsigned char)t.get_or("a", 255)}};
            }
            float x = t.get_or("x", 0.0f), y = t.get_or("y", 0.0f);
            if (t["w"].valid()) return Value{Vector4{x, y, t.get_or("z", 0.0f), t.get_or("w", 0.0f)}};
            if (t["z"].valid()) return Value{Vector3{x, y, t.get_or("z", 0.0f)}};
            return Value{Vector2{x, y}};
        }

        // Uniform value -> Lua. vec4 comes back as a Color (0..255): nearly every vec4
        // material uniform is a color, and it round-trips through Set.
        sol::object ValueToLua(sol::state_view lua, const Value& value) {
            if (value.Is<bool>()) return sol::make_object(lua, value.As<bool>());
            if (value.Is<int>()) return sol::make_object(lua, value.As<int>());
            if (value.Is<float>()) return sol::make_object(lua, value.As<float>());
            if (value.Is<Vector2>()) return sol::make_object(lua, value.As<Vector2>());
            if (value.Is<Vector3>()) {
                Vector3 v = value.As<Vector3>();
                return sol::make_object(lua, lua.create_table_with("x", v.x, "y", v.y, "z", v.z));
            }
            Vector4 v = value.As<Vector4>();
            auto channel = [](float f) { return (unsigned char)std::clamp(f * 255.0f + 0.5f, 0.0f, 255.0f); };
            return sol::make_object(lua, Color{channel(v.x), channel(v.y), channel(v.z), channel(v.w)});
        }
    }

    // Lua:
    //   local mat = GetComponent(entity, "Material")
    //   local flat = mat:Layer("Flat")          -- first layer using that material, or nil
    //   flat:Set("uColor", "#FF8800FF")         -- or a Color, {r,g,b,a}, number, {x,y}
    //   local c = flat:Get("uColor")            -- nil until set (the shader default applies)
    //   flat.enabled = false
    //   mat:LayerAt(1), mat.layerCount           -- positional access, 1-based
    void MaterialComponent::BindLua(sol::usertype<MaterialComponent>& ut) {
        ut["padding"] = &MaterialComponent::padding;
        ut["enabled"] = &MaterialComponent::enabled;
        ut["layerCount"] = sol::readonly_property([](MaterialComponent& c) { return (int)c.layers.size(); });
        ut["Layer"] = [](MaterialComponent& c, const std::string& material) -> MaterialLayer* {
            for (MaterialLayer& layer : c.layers) {
                if (layer.material == material) return &layer;
            }
            return nullptr;
        };
        ut["LayerAt"] = [](MaterialComponent& c, int index) -> MaterialLayer* {
            if (index < 1 || index > (int)c.layers.size()) return nullptr;
            return &c.layers[index - 1];
        };

        sol::state_view lua(ut.lua_state());
        auto layerType = lua.new_usertype<MaterialLayer>("MaterialLayer", sol::no_constructor);
        layerType["material"] = sol::readonly_property([](MaterialLayer& l) { return l.material; });
        layerType["enabled"] = &MaterialLayer::enabled;
        layerType["texture"] = &MaterialLayer::texturePath;
        layerType["Set"] = [](MaterialLayer& l, const std::string& name, sol::object v) {
            if (std::optional<Value> value = ValueFromLua(v)) l.overrides[name] = *value;
        };
        layerType["Get"] = [](MaterialLayer& l, const std::string& name, sol::this_state s) -> sol::object {
            auto it = l.overrides.find(name);
            if (it == l.overrides.end()) return sol::lua_nil;
            return ValueToLua(s, it->second);
        };
        layerType["Clear"] = [](MaterialLayer& l, const std::string& name) { l.overrides.erase(name); };
    }

    REGISTER_COMPONENT(MaterialComponent);
}

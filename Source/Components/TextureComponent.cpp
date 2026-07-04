#include "Components/TextureComponent.h"
#include "Core/Application.h"
#include "Core/ComponentRegistry.h"
#include "Services/AssetService.h"
#include "imgui.h"
#include <cstring>

namespace Elysium {
    static TextureFilterMode ParseFilterMode(const char* s) {
        if (s && strcmp(s, "bilinear") == 0) return TextureFilterMode::Bilinear;
        return TextureFilterMode::Point;
    }

    static const char* FilterModeName(TextureFilterMode m) {
        return m == TextureFilterMode::Bilinear ? "bilinear" : "point";
    }

    void TextureComponent::SaveXml(const TextureComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("TextureComponent")
            .SetAttribute("texture", c.textureName.c_str())
            .SetAttribute("sourceX", c.sourceRect.x)
            .SetAttribute("sourceY", c.sourceRect.y)
            .SetAttribute("sourceWidth", c.sourceRect.width)
            .SetAttribute("sourceHeight", c.sourceRect.height);
        if (c.originX != 0.5f) b.SetAttribute("originX", c.originX);
        if (c.originY != 0.5f) b.SetAttribute("originY", c.originY);
        std::string tintHex = ColorToHex(c.tint);
        if (!tintHex.empty() && tintHex != "#FFFFFFFF") b.SetAttribute("tint", tintHex.c_str());
        if (c.filterMode != TextureFilterMode::Point) b.SetAttribute("filter", FilterModeName(c.filterMode));
    }

    void TextureComponent::LoadXml(TextureComponent& c, tinyxml2::XMLElement* el) {
        c.textureName = el->Attribute("texture") ? el->Attribute("texture") : "";
        if (!c.textureName.empty()) {
            auto& assetService = Application::GetInstance().GetService<Services::AssetService>();
            assetService.LoadAsset(AssetType::TEXTURE, Path(c.textureName));
        }
        c.sourceRect.x = el->FloatAttribute("sourceX", 0.0f);
        c.sourceRect.y = el->FloatAttribute("sourceY", 0.0f);
        c.sourceRect.width = el->FloatAttribute("sourceWidth", 0.0f);
        c.sourceRect.height = el->FloatAttribute("sourceHeight", 0.0f);
        c.originX = el->FloatAttribute("originX", 0.5f);
        c.originY = el->FloatAttribute("originY", 0.5f);
        std::string tintHex = el->Attribute("tint") ? el->Attribute("tint") : "";
        c.tint = ParseHexColor(tintHex, WHITE);
        c.filterMode = ParseFilterMode(el->Attribute("filter"));
    }

    void TextureComponent::Inspect(TextureComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        ImGui::TextDisabled("Resolved (read-only)");
        ImGui::BeginDisabled();

        Label("Texture: ");
        std::string textureId = "##TextureName_" + std::to_string(e);
        char buffer[256];
        std::strncpy(buffer, c.textureName.c_str(), sizeof(buffer));
        ImGui::InputText(textureId.c_str(), buffer, sizeof(buffer));

        Label("Source Rect: ");
        float rect[4] = {c.sourceRect.x, c.sourceRect.y, c.sourceRect.width, c.sourceRect.height};
        std::string rectId = "##TextureSourceRect_" + std::to_string(e);
        ImGui::DragFloat4(rectId.c_str(), rect, 1.0f);

        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::TextDisabled("Per-instance");

        float tint[4] = {c.tint.r / 255.0f, c.tint.g / 255.0f, c.tint.b / 255.0f, c.tint.a / 255.0f};
        Label("Tint: ");
        std::string tintId = "##TextureTint_" + std::to_string(e);
        if (ImGui::ColorEdit4(tintId.c_str(), tint)) {
            c.tint = {(unsigned char)(tint[0] * 255), (unsigned char)(tint[1] * 255),
                      (unsigned char)(tint[2] * 255), (unsigned char)(tint[3] * 255)};
        }

        Label("Origin X: ");
        std::string originXId = "##TextureOriginX_" + std::to_string(e);
        ImGui::DragFloat(originXId.c_str(), &c.originX, 0.01f, 0.0f, 1.0f);

        Label("Origin Y: ");
        std::string originYId = "##TextureOriginY_" + std::to_string(e);
        ImGui::DragFloat(originYId.c_str(), &c.originY, 0.01f, 0.0f, 1.0f);

        Label("Filter: ");
        const char* filterItems[] = { "Point", "Bilinear" };
        int filter = static_cast<int>(c.filterMode);
        std::string filterId = "##TextureFilter_" + std::to_string(e);
        if (ImGui::Combo(filterId.c_str(), &filter, filterItems, 2))
            c.filterMode = static_cast<TextureFilterMode>(filter);
    }

    void TextureComponent::BindLua(sol::usertype<TextureComponent>& ut) {
        ut["textureName"] = &TextureComponent::textureName;
        ut["tint"] = sol::property(
            [](TextureComponent& t) { return t.tint; },
            [](TextureComponent& t, Color v) { t.tint = v; });
        ut["originX"] = &TextureComponent::originX;
        ut["originY"] = &TextureComponent::originY;
    }

    REGISTER_COMPONENT(TextureComponent);
}

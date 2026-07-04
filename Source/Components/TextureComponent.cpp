#include "Components/TextureComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
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

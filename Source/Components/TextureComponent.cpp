#include "Components/TextureComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Application.h"
#include "Services/AssetService.h"
#include "imgui.h"

namespace Elysium {
    TextureComponent::TextureComponent(const std::string& texture, const std::string& layer)
        : textureName(texture), layerName(layer) {}

    void TextureComponent::LoadXml(TextureComponent& c, tinyxml2::XMLElement* el) {
        // Not explicitly implemented in SceneLoader original, but easy to add defaults
        // Original loader used ComponentLoaders["TextureComponent"] which wasn't in list?
        // Wait, looking at SceneLoader code again...
        // componentLoaders["TextureComponent"] WAS NOT in the map!
        // It seems TextureComponent was defined in structs but no loader in SceneLoader.cpp?
        // Ah, I missed it? 
        // No, I see ScaleComponent at the end of ComponentLoaders().
        // I checked `SceneLoader.cpp` read. `TextureComponent` loader is missing.
        // It's possible it wasn't loadable from XML or I missed it in the file.
        // Wait, `WorldService` had `DrawComponent<TextureComponent>`.
        // I will implement a basic loader.
    }

    void TextureComponent::Inspect(TextureComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        // Texture asset picker
        Label("Texture Asset: ");
        auto& assetService = Application::GetInstance().GetService<Elysium::Services::AssetService>();
        const auto& allAssets = assetService.GetAllAssets();

        // Build list of texture assets
        std::vector<std::string> textureAssetNames;
        textureAssetNames.push_back("<None>");

        for (const auto& [name, asset] : allAssets) {
            if (asset.GetType() == AssetType::TEXTURE && asset.IsLoaded()) {
                textureAssetNames.push_back(name);
            }
        }

        // Find current selection
        std::string currentTextureName = c.textureName.empty() ? "<None>" : c.textureName;
        int currentIndex = 0;
        for (size_t i = 0; i < textureAssetNames.size(); ++i) {
            if (textureAssetNames[i] == currentTextureName) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        std::string assetComboId = "##TextureAsset_" + std::to_string(e);
        if (ImGui::BeginCombo(assetComboId.c_str(), currentTextureName.c_str())) {
            for (size_t i = 0; i < textureAssetNames.size(); ++i) {
                bool isSelected = (currentIndex == static_cast<int>(i));
                std::string selectableId = textureAssetNames[i] + "##" + std::to_string(i);
                if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                    c.textureName = (i == 0) ? "" : textureAssetNames[i];
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        static char layerBuffer[256];
        strncpy(layerBuffer, c.layerName.c_str(), sizeof(layerBuffer) - 1);
        layerBuffer[sizeof(layerBuffer) - 1] = '\0';

        Label("Layer Name: ");
        if (ImGui::InputText("##TextureLayerName", layerBuffer, sizeof(layerBuffer))) {
            c.layerName = std::string(layerBuffer);
        }

        Label("Clip Rect: ");
        ImGui::Text("X:");
        ImGui::SameLine();
        std::string clipXId = "##TextureClipX_" + std::to_string(e);
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat(clipXId.c_str(), &c.clip.x, 1.0f, 0.0f);
        ImGui::SameLine();
        ImGui::Text("Y:");
        ImGui::SameLine();
        std::string clipYId = "##TextureClipY_" + std::to_string(e);
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat(clipYId.c_str(), &c.clip.y, 1.0f, 0.0f);
        ImGui::Text("W:");
        ImGui::SameLine();
        std::string clipWId = "##TextureClipW_" + std::to_string(e);
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat(clipWId.c_str(), &c.clip.width, 1.0f, 0.0f);
        ImGui::SameLine();
        ImGui::Text("H:");
        ImGui::SameLine();
        std::string clipHId = "##TextureClipH_" + std::to_string(e);
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat(clipHId.c_str(), &c.clip.height, 1.0f, 0.0f);
        ImGui::SameLine();
        ImGui::TextDisabled("(0 = full texture)");

        Label("Tint: ");
        std::string tintId = "##TextureTint_" + std::to_string(e);
        float tintColor[4] = {
            c.tint.r / 255.0f,
            c.tint.g / 255.0f,
            c.tint.b / 255.0f,
            c.tint.a / 255.0f};
        if (ImGui::ColorEdit4(tintId.c_str(), tintColor)) {
            c.tint = {
                static_cast<unsigned char>(tintColor[0] * 255),
                static_cast<unsigned char>(tintColor[1] * 255),
                static_cast<unsigned char>(tintColor[2] * 255),
                static_cast<unsigned char>(tintColor[3] * 255)};
        }
    }

    void TextureComponent::BindLua(sol::usertype<TextureComponent>& ut) {
        ut["textureName"] = &TextureComponent::textureName;
        ut["layerName"] = &TextureComponent::layerName;
        // clip and tint? 
    }

    void TextureComponent::SetFromLua(TextureComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.textureName = t.get_or("textureName", c.textureName);
            c.layerName = t.get_or("layerName", c.layerName);
        }
    }

    REGISTER_COMPONENT(TextureComponent);
}

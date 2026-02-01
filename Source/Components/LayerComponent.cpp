#include "Components/LayerComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"
#include "tinyxml2.h"

namespace Elysium {
    LayerComponent::LayerComponent(int z) 
        : zIndex(z), type(Type::World), space(Space::World), blend(Blend::Normal), 
          opacity(1.0f), isVisible(true), name("default") {}

    static LayerComponent::Type ParseLayerType(const char* typeStr) {
        if (!typeStr) return LayerComponent::Type::World;
        std::string type = typeStr;
        if (type == "Background") return LayerComponent::Type::Background;
        if (type == "World") return LayerComponent::Type::World;
        if (type == "Lighting") return LayerComponent::Type::Lighting;
        if (type == "Overlay") return LayerComponent::Type::Overlay;
        return LayerComponent::Type::World;
    }

    static LayerComponent::Space ParseLayerSpace(const char* spaceStr) {
        if (!spaceStr) return LayerComponent::Space::World;
        std::string space = spaceStr;
        if (space == "Screen") return LayerComponent::Space::Screen;
        if (space == "World") return LayerComponent::Space::World;
        if (space == "Parallax") return LayerComponent::Space::Parallax;
        return LayerComponent::Space::World;
    }

    static LayerComponent::Blend ParseLayerBlend(const char* blendStr) {
        if (!blendStr) return LayerComponent::Blend::Normal;
        std::string blend = blendStr;
        if (blend == "Normal") return LayerComponent::Blend::Normal;
        if (blend == "Additive") return LayerComponent::Blend::Additive;
        if (blend == "Multiply") return LayerComponent::Blend::Multiply;
        if (blend == "Alpha") return LayerComponent::Blend::Alpha;
        return LayerComponent::Blend::Normal;
    }

    void LayerComponent::LoadXml(LayerComponent& c, tinyxml2::XMLElement* el) {
        const char* name = el->Attribute("name");
        c.name = name ? name : "default";
        c.zIndex = el->IntAttribute("z", 0);
        c.opacity = el->FloatAttribute("opacity", 1.0f);
        c.isVisible = el->BoolAttribute("isVisible", true);

        c.type = ParseLayerType(el->Attribute("type"));
        c.space = ParseLayerSpace(el->Attribute("space"));
        c.blend = ParseLayerBlend(el->Attribute("blend"));
    }

    void LayerComponent::Inspect(LayerComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Z Index: ");
        ImGui::DragInt("##ZIndex", &c.zIndex);

        const char* types[] = {"Background", "World", "Lighting", "Overlay"};
        int typeIndex = static_cast<int>(c.type);
        Label("Type: ");
        if (ImGui::Combo("##Type", &typeIndex, types, IM_ARRAYSIZE(types))) {
            c.type = static_cast<LayerComponent::Type>(typeIndex);
        }

        const char* spaces[] = {"World", "Screen", "Parallax"};
        int spaceIndex = static_cast<int>(c.space);
        Label("Space: ");
        if (ImGui::Combo("##Space", &spaceIndex, spaces, IM_ARRAYSIZE(spaces))) {
            c.space = static_cast<LayerComponent::Space>(spaceIndex);
        }

        const char* blends[] = {"Normal", "Additive", "Multiply", "Alpha"};
        int blendIndex = static_cast<int>(c.blend);
        Label("Blend: ");
        if (ImGui::Combo("##Blend", &blendIndex, blends, IM_ARRAYSIZE(blends))) {
            c.blend = static_cast<LayerComponent::Blend>(blendIndex);
        }

        Label("Opacity: ");
        ImGui::DragFloat("##Opacity", &c.opacity, 0.01f, 0.0f, 1.0f);
        Label("Is Visible: ");
        ImGui::Checkbox("##IsVisible", &c.isVisible);

        static char nameBuffer[256];
        strncpy(nameBuffer, c.name.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';

        Label("Name: ");
        if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
            c.name = std::string(nameBuffer);
        }

        Label("Parallax: ");
        ImGui::DragFloat2("##ParallaxFactor", &c.parallaxFactor.x, 0.01f, 0.0f, 1.0f);
    }

    void LayerComponent::BindLua(sol::usertype<LayerComponent>& ut) {
        ut["name"] = &LayerComponent::name;
        ut["zIndex"] = &LayerComponent::zIndex;
        ut["opacity"] = &LayerComponent::opacity;
        ut["isVisible"] = &LayerComponent::isVisible;
    }

    void LayerComponent::SetFromLua(LayerComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.name = t.get_or("name", c.name);
            c.zIndex = t.get_or("zIndex", c.zIndex);
            c.opacity = t.get_or("opacity", c.opacity);
            c.isVisible = t.get_or("isVisible", c.isVisible);
        }
    }

    REGISTER_COMPONENT(LayerComponent);
}

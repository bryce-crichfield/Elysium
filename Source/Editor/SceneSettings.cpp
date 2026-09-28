#include "Editor/SceneSettings.h"
#include <algorithm>
#include "Core/Common.h"
#include "Core/Scene.h"
#include "Core/System.h"
#include "Editor/AssetStyle.h"
#include "Editor/Widgets.h"
#include "Interfaces/ISceneService.h"

namespace Elysium {

using namespace Services;

namespace {
// An eye toggle at the right edge of the header just drawn.
void VisibilityToggle(bool& visible, const char* tooltip) {
    const char* icon = visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
    AlignRight(ButtonWidth(icon));
    ImGui::PushStyleColor(ImGuiCol_Text, visible ? Editor::Palette().Text : Editor::Palette().TextDisabled);
    // Stable ID: the icon flips with the state.
    if (IconButton((std::string(icon) + "##visible").c_str(), tooltip)) visible = !visible;
    ImGui::PopStyleColor();
}

template <typename Enum>
void EnumRow(const char* label, Enum& field, const char* const* names, int count) {
    PropertyLabel(label);
    int index = static_cast<int>(field);
    if (ImGui::Combo((std::string("##") + label).c_str(), &index, names, count)) field = static_cast<Enum>(index);
}
}  // namespace

void SceneSettings::Draw(Scene& scene) {
    Profile;

    auto& service = services_.Get<ISceneService>();
    BeginKindSettings(AssetKind::Scene, "SceneSettings", service.GetSceneName(&scene) + " Settings");
    DrawProperties(service, scene);
    DrawLayers(scene);
    DrawSystems(scene);
    EndKindSettings();
}

void SceneSettings::DrawProperties(ISceneService& service, Scene& scene) {
    const std::string name = service.GetSceneName(&scene);
    const auto& config = scene.GetConfiguration();

    SectionHeader("Properties");
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.0f x %.0f", config.resolutionWidth, config.resolutionHeight);
    ReadOnlyRow("Resolution", buffer);

    const auto& stack = service.GetStack();
    const auto position = std::find(stack.begin(), stack.end(), &scene) - stack.begin();
    if (position == (long long)stack.size() - 1) snprintf(buffer, sizeof(buffer), "Top of %zu", stack.size());
    else snprintf(buffer, sizeof(buffer), "%lld of %zu", (long long)position + 1, stack.size());
    ReadOnlyRow("Stack", buffer);

    auto registration = service.GetSceneRegistry().find(name);
    if (registration != service.GetSceneRegistry().end() && !registration->second.xmlPath.empty()) {
        ReadOnlyRow("Source", registration->second.xmlPath.c_str());
    }
}

void SceneSettings::DrawLayers(Scene& scene) {
    SectionHeader("Layers");

    auto& layers = scene.GetLayers();
    if (layers.empty()) {
        ImGui::TextDisabled("No layers");
        return;
    }

    static const char* spaceNames[] = {"World", "Screen"};
    static const char* blendNames[] = {"Normal", "Additive", "Multiply"};

    bool needsSort = false;
    for (size_t i = 0; i < layers.size(); ++i) {
        SceneLayer& layer = layers[i];
        ImGui::PushID("layer");
        ImGui::PushID((int)i);

        const bool open = CollapsingSection((layer.name + "  (z " + std::to_string(layer.zIndex) + ")###layer").c_str(), layer.isVisible);
        VisibilityToggle(layer.isVisible, layer.isVisible ? "Hide layer" : "Show layer");

        if (open) {
            BeginSectionBody();
            PropertyLabel("Z Index");
            int zIndex = layer.zIndex;
            if (ImGui::InputInt("##zIndex", &zIndex) && zIndex != layer.zIndex) {
                auto clash = std::find_if(layers.begin(), layers.end(),
                                          [&](const SceneLayer& other) { return &other != &layer && other.zIndex == zIndex; });
                if (clash != layers.end()) {
                    zIndexError_ = "Z " + std::to_string(zIndex) + " is already used by '" + clash->name + "'";
                } else {
                    layer.zIndex = zIndex;
                    zIndexError_.clear();
                    needsSort = true;
                }
            }

            EnumRow("Space", layer.space, spaceNames, IM_ARRAYSIZE(spaceNames));
            EnumRow("Blend", layer.layerBlend, blendNames, IM_ARRAYSIZE(blendNames));
            EnumRow("Composite", layer.compositeBlend, blendNames, IM_ARRAYSIZE(blendNames));

            PropertyLabel("Offscreen");
            ImGui::Checkbox("##composited", &layer.isComposited);
            ItemTooltip("Render into its own target, then composite onto the frame");

            PropertyLabel("Opacity");
            ImGui::SliderFloat("##opacity", &layer.opacity, 0.0f, 1.0f, "%.2f");

            PropertyLabel("Ambient");
            float ambient[4] = {layer.ambient.r / 255.0f, layer.ambient.g / 255.0f, layer.ambient.b / 255.0f,
                                layer.ambient.a / 255.0f};
            if (ImGui::ColorEdit4("##ambient", ambient)) {
                layer.ambient = {(unsigned char)(ambient[0] * 255), (unsigned char)(ambient[1] * 255),
                                 (unsigned char)(ambient[2] * 255), (unsigned char)(ambient[3] * 255)};
            }
            ItemTooltip("What an offscreen layer is cleared to before drawing");

            SectionHeader("Lighting");
            PropertyLabel("Lit");
            ImGui::Checkbox("##lit", &layer.isLit);
            ItemTooltip("Shade this layer: glowing materials light what's around them, by its normal maps");
            if (layer.isLit) {
                auto colorRow = [](const char* label, const char* id, Color& color) {
                    PropertyLabel(label);
                    float rgba[4] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
                    if (ImGui::ColorEdit4(id, rgba)) {
                        color = {(unsigned char)(rgba[0] * 255), (unsigned char)(rgba[1] * 255),
                                 (unsigned char)(rgba[2] * 255), (unsigned char)(rgba[3] * 255)};
                    }
                };
                colorRow("Ambient Light", "##lightAmbient", layer.lightAmbient);
                ItemTooltip("Light everywhere on the layer. White looks unlit");
                PropertyLabel("Reach");
                ImGui::DragFloat("##lightReach", &layer.lightReach, 1.0f, 1.0f, 4000.0f, "%.0f");
                ItemTooltip("How far a glowing material's light carries, in world units");
                PropertyLabel("Height");
                ImGui::DragFloat("##lightHeight", &layer.lightHeight, 0.5f, 1.0f, 1000.0f, "%.0f");
                ItemTooltip("How far above the layer glowing pixels sit. Lower rakes across normal maps");
                PropertyLabel("Strength");
                ImGui::DragFloat("##lightStrength", &layer.lightStrength, 0.01f, 0.0f, 20.0f, "%.2f");
                PropertyLabel("Bands");
                ImGui::SliderInt("##lightBands", &layer.lightBands, 0, 8, layer.lightBands == 0 ? "Smooth" : "%d");
                ItemTooltip("Quantize light into steps for a cel-shaded look");
                PropertyLabel("Outline");
                ImGui::SliderFloat("##outline", &layer.outline, 0.0f, 1.0f, "%.2f");
                ItemTooltip("Ink lines at silhouettes and creases in the normals");
                if (layer.outline > 0.0f) colorRow("Outline Color", "##outlineColor", layer.outlineColor);
            }
            EndSectionBody();
        }
        ImGui::PopID();
        ImGui::PopID();
    }

    // Sorted after the loop so the rows being drawn don't move under it.
    if (needsSort) {
        std::sort(layers.begin(), layers.end(), [](const SceneLayer& a, const SceneLayer& b) { return a.zIndex < b.zIndex; });
    }
    if (!zIndexError_.empty()) {
        ColoredText(Editor::Palette().Error, (ICON_FA_CIRCLE_XMARK "  " + zIndexError_).c_str());
    }
}

void SceneSettings::DrawSystems(Scene& scene) {
    SectionHeader("Systems");

    const auto& systems = scene.GetSystems();
    if (systems.empty()) {
        ImGui::TextDisabled("No systems");
        return;
    }

    for (size_t i = 0; i < systems.size(); ++i) {
        System& system = *systems[i];
        ImGui::PushID("system");
        ImGui::PushID((int)i);

        bool isEnabled = system.IsEnabled();
        const bool open = CollapsingSection((system.GetName() + "###system").c_str(), isEnabled);

        // Header controls, right to left: drawing toggle, then the enabled checkbox.
        bool isVisible = system.IsVisible();
        VisibilityToggle(isVisible, isVisible ? "Hide drawing" : "Show drawing");
        if (isVisible != system.IsVisible()) system.SetVisible(isVisible);
        AlignRight(ImGui::GetFrameHeight() + ButtonWidth(ICON_FA_EYE) + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Checkbox("##enabled", &isEnabled)) system.SetEnabled(isEnabled);
        ItemTooltip(isEnabled ? "Disable updates" : "Enable updates");

        if (open) {
            BeginSectionBody();
            DrawSystemParameters(system);
            EndSectionBody();
        }
        ImGui::PopID();
        ImGui::PopID();
    }
}

// Edits apply live. Scene > Save writes whichever differ from the default back onto the
// <System> tag.
void SceneSettings::DrawSystemParameters(System& system) {
    const SystemParameters defaults = system.GetDefaultParameters();
    if (defaults.empty()) {
        ImGui::TextDisabled("No parameters");
        return;
    }

    // Copied: SetParameter below may rewrite the map being iterated.
    const SystemParameters parameters = system.GetParameters();
    for (const auto& [name, current] : parameters) {
        auto defaultIt = defaults.find(name);
        if (defaultIt == defaults.end()) continue;
        Value value = current;
        if (InspectValueRow(name, value, defaultIt->second, current == defaultIt->second)) system.SetParameter(name, value);
    }
}

}  // namespace Elysium

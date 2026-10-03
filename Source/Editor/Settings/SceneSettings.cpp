#include "Editor/Settings/SceneSettings.h"
#include <algorithm>
#include "Core/Common.h"
#include "Core/Scene.h"
#include "Core/System.h"
#include "Editor/Style/AssetStyle.h"
#include "Editor/Widgets/Widgets.h"
#include "Editor/EditorApplication.h"
#include "Interfaces/ISceneService.h"

namespace Elysium {

using namespace Services;

SceneSettings::SceneSettings(EditorApplication& editor) : editor_(editor), services_(editor.GetServices()) {}

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
    DrawGrid();
    DrawLayers(scene);
    DrawSystems(scene);
    EndKindSettings();
}

void SceneSettings::DrawProperties(ISceneService& service, Scene& scene) {
    SectionHeader("Properties");

    // The name is the file's: renaming renames Scenes/<name>.xml. Anything that pushes the scene
    // by its old name (a script's Push("...")) has to be updated by hand.
    if (nameBufferScene_ != &scene) {
        snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", scene.GetName().c_str());
        nameBufferScene_ = &scene;
    }
    PropertyLabel("Name");
    if (ImGui::InputText("##sceneName", nameBuffer_, sizeof(nameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (!editor_.RenameScene(scene, nameBuffer_)) snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", scene.GetName().c_str());
    }
    // Clicking away abandons the edit.
    if (!ImGui::IsItemActive() && scene.GetName() != nameBuffer_) {
        snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", scene.GetName().c_str());
    }
    ItemTooltip("Renames the scene's file (Enter to apply). Scripts that push it by name need updating");
    if (service.GetEntryScene() == scene.GetName()) ReadOnlyRow("Entry", "The project starts here");

    SceneConfiguration config = scene.GetConfiguration();
    PropertyLabel("Resolution");
    float resolution[2] = {config.resolutionWidth, config.resolutionHeight};
    if (ImGui::DragFloat2("##resolution", resolution, 1.0f, 1.0f, 8192.0f, "%.0f")) {
        config.resolutionWidth = std::max(1.0f, resolution[0]);
        config.resolutionHeight = std::max(1.0f, resolution[1]);
        scene.SetConfiguration(config);
    }

    if (!scene.GetPath().empty()) ReadOnlyRow("Source", scene.GetPath().c_str());
}

void SceneSettings::DrawGrid() {
    auto& grid = editor_.GetGrid();

    SectionHeader("Grid");
    MutedText("Saved with the scene, for the editor only");

    static const char* kLattices[] = {"Square", "Isometric"};
    EnumRow("Lattice", grid.lattice, kLattices, IM_ARRAYSIZE(kLattices));

    PropertyLabel(grid.lattice == GridLattice::Isometric ? "Diamond Width" : "Cell Width");
    ImGui::DragFloat("##gridWidth", &grid.width, 1.0f, 1.0f, 4096.0f, "%.0f");
    PropertyLabel(grid.lattice == GridLattice::Isometric ? "Diamond Height" : "Cell Height");
    ImGui::DragFloat("##gridHeight", &grid.height, 1.0f, 1.0f, 4096.0f, "%.0f");

    // Subdividing beats resizing when you want half-tile placement but the same visible grid.
    PropertyLabel("Subdivide");
    static const char* kDivisors[] = {"1", "2", "4", "8"};
    const int values[] = {1, 2, 4, 8};
    int index = 0;
    for (int i = 0; i < IM_ARRAYSIZE(values); ++i) {
        if (values[i] == grid.divisor) index = i;
    }
    if (ImGui::Combo("##gridDivisor", &index, kDivisors, IM_ARRAYSIZE(kDivisors))) grid.divisor = values[index];

    PropertyLabel("Snap");
    ImGui::Checkbox("##gridSnap", &grid.snapEnabled);
    PropertyLabel("Show");
    ImGui::Checkbox("##gridShow", &grid.showGrid);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.1f x %.1f", grid.Cell().x, grid.Cell().y);
    ReadOnlyRow("Snaps to", buffer);
}

void SceneSettings::DrawLayers(Scene& scene) {
    SectionHeader("Layers");

    auto& layers = scene.GetLayers();
    if (layers.empty()) {
        ImGui::TextDisabled("No layers");
        return;
    }

    static const char* spaceNames[] = {"World", "Screen"};  // SceneLayerSpace order
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
            if (layer.space == SceneLayerSpace::World3D) {
                PropertyLabel("Ground");
                ImGui::Checkbox("##ground", &layer.ground);
                ItemTooltip("Lies flat on the ground (selection rings, shadows, markers) instead of standing in the world");
            }
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

            if (layer.IsLit()) {
                SectionHeader("Lighting");
                auto colorRow = [](const char* label, const char* id, Color& color) {
                    PropertyLabel(label);
                    float rgba[4] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
                    if (ImGui::ColorEdit4(id, rgba)) {
                        color = {(unsigned char)(rgba[0] * 255), (unsigned char)(rgba[1] * 255),
                                 (unsigned char)(rgba[2] * 255), (unsigned char)(rgba[3] * 255)};
                    }
                };
                colorRow("Ambient Light", "##lightAmbient", layer.lightAmbient);
                ItemTooltip("Light everywhere, before any Light component. White looks unlit");
                colorRow("Sun", "##sunColor", layer.sunColor);
                ItemTooltip("Light from one direction everywhere, like daylight or a moon. Black: off");
                if (layer.sunColor.r || layer.sunColor.g || layer.sunColor.b) {
                    PropertyLabel("Sun Intensity");
                    ImGui::DragFloat("##sunIntensity", &layer.sunIntensity, 0.01f, 0.0f, 8.0f, "%.2f");
                    PropertyLabel("Sun Direction");
                    ImGui::DragFloat("##sunYaw", &layer.sunYaw, 0.5f, -360.0f, 360.0f, "%.0f deg around");
                    PropertyLabel("Sun Height");
                    ImGui::SliderFloat("##sunPitch", &layer.sunPitch, 1.0f, 90.0f, "%.0f deg up");
                }
                PropertyLabel("Rim Light");
                ImGui::SliderFloat("##rimLight", &layer.rimLight, 0.0f, 2.0f, "%.2f");
                ItemTooltip("A bright edge on models where they turn away from the camera. 0: off");
                PropertyLabel("Shadows");
                ImGui::Checkbox("##shadows", &layer.shadows);
                ItemTooltip("Models cast shadows from the lights");
                if (layer.shadows) {
                    PropertyLabel("Shadow Bias");
                    ImGui::DragFloat("##shadowBias", &layer.shadowBias, 0.1f, 0.0f, 64.0f, "%.1f");
                    ItemTooltip("How far a point lifts off its own surface before testing shadows. Too low: walls shadow themselves");
                }
                PropertyLabel("Fog of War");
                ImGui::SliderFloat("##fogOfWar", &layer.fogOfWar, 0.0f, 1.0f, "%.2f");
                ItemTooltip("How far what no Vision light can see fades into the fog color. 0: off");
                if (layer.fogOfWar > 0.0f) colorRow("Fog Color", "##fogColor", layer.fogColor);
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

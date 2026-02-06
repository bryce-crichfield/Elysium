#include "SceneEditor.h"
#include <algorithm>
#include <typeinfo>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/SceneLayer.h"
#include "Core/System.h"
#include "Services/SceneService.h"
#include "imgui.h"

namespace Elysium {

using namespace Services;

SceneEditor::SceneEditor() : Editor("Scene Editor") {}

void SceneEditor::Draw(Application& app) {
    Profile;

    auto& service = app.GetService<SceneService>();

    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(name_.c_str(), nullptr, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginTabBar("SceneEditorTabs")) {
            if (ImGui::BeginTabItem("Scenes")) {
                DrawScenesTab(service);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Scene")) {
                DrawSceneTab(service);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Systems")) {
                DrawSystemsTab(service);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void SceneEditor::DrawScenesTab(SceneService& service) {
    // Scene Management Buttons
    bool hasSelection = selectedSceneIndex_ >= 0 && selectedSceneIndex_ < (int)service.scenes_.size();

    if (ImGui::Button("Push") && hasSelection) {
        auto it = service.scenes_.begin();
        std::advance(it, selectedSceneIndex_);
        service.Push(it->first);
    }
    if (!hasSelection) {
        ImGui::SetItemTooltip("Select a scene to push onto stack");
    }

    ImGui::SameLine();
    if (ImGui::Button("Replace") && hasSelection) {
        auto it = service.scenes_.begin();
        std::advance(it, selectedSceneIndex_);
        service.Replace(it->first);
    }
    if (!hasSelection) {
        ImGui::SetItemTooltip("Select a scene to replace top of stack");
    }

    ImGui::SameLine();
    if (ImGui::Button("Pop")) {
        service.Pop();
    }
    if (service.IsEmpty()) {
        ImGui::SetItemTooltip("Stack is empty");
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        service.Clear();
    }

    ImGui::Separator();

    // Scene Registry Table
    ImGui::Text("Scene Registry:");
    if (ImGui::BeginTable("SceneTable", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0, 200))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("XML Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("In Stack", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        int index = 0;
        for (const auto& [name, sceneData] : service.scenes_) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(name.c_str(), selectedSceneIndex_ == index, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedSceneIndex_ = index;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", sceneData.xmlPath.c_str());

            ImGui::TableSetColumnIndex(2);
            bool inStack = false;
            for (Scene* s : service.sceneStack_) {
                if (s == sceneData.scene) {
                    inStack = true;
                    break;
                }
            }
            ImGui::Text("%s", inStack ? "Yes" : "No");

            index++;
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    // Scene Stack Table
    ImGui::Text("Scene Stack (top to bottom):");
    if (ImGui::BeginTable("StackTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Scene Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const auto& stack = service.sceneStack_;
        for (int i = (int)stack.size() - 1; i >= 0; --i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (i == (int)stack.size() - 1) {
                ImGui::Text("TOP");
            } else if (i == 0) {
                ImGui::Text("BOTTOM");
            } else {
                ImGui::Text("%d", i);
            }

            ImGui::TableSetColumnIndex(1);
            std::string sceneName = "Unknown";
            for (const auto& [name, data] : service.scenes_) {
                if (data.scene == stack[i]) {
                    sceneName = name;
                    break;
                }
            }
            bool isSelected = (stack[i] == service.GetScene());
            if (ImGui::Selectable(sceneName.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                service.SetSelectedScene(stack[i]);
            }
        }

        if (stack.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("-");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("(empty)");
        }

        ImGui::EndTable();
    }
}

void SceneEditor::DrawSceneTab(SceneService& service) {
    Scene* topScene = service.GetScene();
    if (!topScene) {
        ImGui::Text("No scenes in stack");
        ImGui::Text("Select a scene from the Scenes tab and click 'Push' to begin.");
        return;
    }

    // Find scene name
    std::string topSceneName = "Unknown";
    for (const auto& [name, data] : service.scenes_) {
        if (data.scene == topScene) {
            topSceneName = name;
            break;
        }
    }

    // Header
    ImGui::Text("Scene: %s", topSceneName.c_str());
    ImGui::Text("Stack Size: %zu", service.GetStackSize());
    ImGui::Checkbox("Pause Updates", &service.paused_);
    ImGui::Separator();

    // Scene Configuration
    const auto& config = topScene->GetConfiguration();
    ImGui::Text("Resolution: %.0f x %.0f", config.resolutionWidth, config.resolutionHeight);
    ImGui::Separator();

    // Layers Table
    auto& layers = topScene->GetLayers();
    if (layers.empty()) {
        ImGui::Text("No layers in current scene");
        return;
    }

    ImGui::Text("Layers:");

    static const char* spaceNames[] = {"World2D", "Screen2D"};
    static const char* blendNames[] = {"Normal", "Additive", "Multiply", "Alpha"};

    if (ImGui::BeginTable("LayersTable", 8,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0, 250))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Z-Index", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Space", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Layer Blend", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Composite Blend", ImGuiTableColumnFlags_WidthFixed, 130);
        ImGui::TableSetupColumn("Ambient", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Composited, ImGuiTableColumnFlags_WidthFixed, 90");
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        bool needsSort = false;

        for (size_t i = 0; i < layers.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::PushID((int)i);

            // Name (read-only)
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", layers[i].name.c_str());

            // Z-Index (editable)
            ImGui::TableSetColumnIndex(1);
            int zIndex = layers[i].zIndex;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputInt("##zIndex", &zIndex, 0, 0) && zIndex != layers[i].zIndex) {
                // Check for duplicates
                bool conflict = false;
                for (size_t j = 0; j < layers.size(); ++j) {
                    if (j != i && layers[j].zIndex == zIndex) {
                        zIndexError_ = "Z-Index " + std::to_string(zIndex) + " already used by layer '" + layers[j].name + "'";
                        conflict = true;
                        break;
                    }
                }
                if (!conflict) {
                    layers[i].zIndex = zIndex;
                    zIndexError_.clear();
                    needsSort = true;
                }
            }

            // Space (combo)
            ImGui::TableSetColumnIndex(2);
            int spaceIdx = static_cast<int>(layers[i].space);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##space", &spaceIdx, spaceNames, IM_ARRAYSIZE(spaceNames))) {
                layers[i].space = static_cast<SceneLayerSpace>(spaceIdx);
            }

            // Blend (combo)
            ImGui::TableSetColumnIndex(3);
            int blendIdx = static_cast<int>(layers[i].layerBlend);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##layerBlend", &blendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
                layers[i].layerBlend = static_cast<SceneLayerBlend>(blendIdx);
            }

            ImGui::TableSetColumnIndex(4);
            int compositeBlendIdx = static_cast<int>(layers[i].compositeBlend);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##compositeBlend", &compositeBlendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
                layers[i].compositeBlend = static_cast<SceneLayerBlend>(compositeBlendIdx);
            }

            // Ambient (color picker)
            ImGui::TableSetColumnIndex(5);
            float ambient[4] = {
                layers[i].ambient.r / 255.0f,
                layers[i].ambient.g / 255.0f,
                layers[i].ambient.b / 255.0f,
                layers[i].ambient.a / 255.0f
            };

            ImGui::SetNextItemWidth(-1);
            if (ImGui::ColorEdit4("##ambient", ambient, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                layers[i].ambient = {
                    (unsigned char)(ambient[0] * 255),
                    (unsigned char)(ambient[1] * 255),
                    (unsigned char)(ambient[2] * 255),
                    (unsigned char)(ambient[3] * 255)
                };
            }

            // Composited (checkbox)
            ImGui::TableSetColumnIndex(6);
            ImGui::Checkbox("##composited", &layers[i].isComposited);

            // Visible (checkbox)
            ImGui::TableSetColumnIndex(7);
            ImGui::Checkbox("##visible", &layers[i].isVisible);

            ImGui::PopID();
        }
        ImGui::EndTable();

        if (needsSort) {
            std::sort(layers.begin(), layers.end(), [](const SceneLayer& a, const SceneLayer& b) {
                return a.zIndex < b.zIndex;
            });
        }
    }

    if (!zIndexError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", zIndexError_.c_str());
    }
}

void SceneEditor::DrawSystemsTab(SceneService& service) {
    Scene* topScene = service.GetScene();
    if (!topScene) {
        ImGui::Text("No active scene");
        return;
    }

    const auto& systems = topScene->GetSystems();
    if (systems.empty()) {
        ImGui::Text("No systems in current scene");
        return;
    }

    if (ImGui::BeginTable("SystemsTable", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0, 200))) {
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("System Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < systems.size(); ++i) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", i);

            ImGui::TableSetColumnIndex(1);
            const std::type_info& typeInfo = typeid(*systems[i]);
            std::string systemName = typeInfo.name();

            size_t lastColon = systemName.find_last_of(':');
            if (lastColon != std::string::npos && lastColon < systemName.length() - 1) {
                systemName = systemName.substr(lastColon + 1);
            }

            ImGui::Text("%s", systemName.c_str());

            ImGui::TableSetColumnIndex(2);
            bool isEnabled = systems[i]->IsEnabled();
            if (ImGui::Checkbox(("##enabled" + std::to_string(i)).c_str(), &isEnabled)) {
                systems[i]->SetEnabled(isEnabled);
            }

            ImGui::TableSetColumnIndex(3);
            bool isVisible = systems[i]->IsVisible();
            if (ImGui::Checkbox(("##visible" + std::to_string(i)).c_str(), &isVisible)) {
                systems[i]->SetVisible(isVisible);
            }
        }

        ImGui::EndTable();
    }

    ImGui::Text("Total Systems: %zu", systems.size());
}

}  // namespace Elysium

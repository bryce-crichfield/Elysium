#include "SceneInspector.h"
#include "Services/SceneService.h"
#include "System.h"
#include "Common.h"
#include "imgui.h"
#include <typeinfo>

namespace Elysium {

using namespace Services;

void SceneInspector::DrawUI(SceneService& service)
{
    Profile;

    // Left side header
    ImGui::Text("Scenes");
    ImGui::SameLine(leftPanelWidth_ + 10);
    ImGui::Text("Scene Stack");

    // Left panel - Scenes Panel
    ImGui::BeginChild("ScenesPanel", ImVec2(leftPanelWidth_, 0), true);
    DrawScenesPanel(service);
    ImGui::EndChild();

    ImGui::SameLine();

    // Splitter
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::Button("##splitter", ImVec2(4.0f, -1));

    if (ImGui::IsItemActive())
    {
        if (!isDraggingSplitter_)
        {
            isDraggingSplitter_ = true;
        }
        else
        {
            leftPanelWidth_ += ImGui::GetIO().MouseDelta.x;
            if (leftPanelWidth_ < 200.0f)
                leftPanelWidth_ = 200.0f;
            if (leftPanelWidth_ > ImGui::GetWindowWidth() - 200.0f)
                leftPanelWidth_ = ImGui::GetWindowWidth() - 200.0f;
        }
    }
    else
    {
        isDraggingSplitter_ = false;
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    // Right panel - Scene Stack Panel
    ImGui::BeginChild("SceneStackPanel", ImVec2(0, 0), true);
    DrawCurrentScenePanel(service);
    ImGui::EndChild();
}

void SceneInspector::DrawScenesPanel(SceneService& service)
{
    // Scene Management Buttons
    bool hasSelection = selectedSceneIndex_ >= 0 && selectedSceneIndex_ < (int)service.scenes_.size();

    if (ImGui::Button("Push") && hasSelection)
    {
        auto it = service.scenes_.begin();
        std::advance(it, selectedSceneIndex_);
        service.Push(it->first);
    }
    if (!hasSelection)
    {
        ImGui::SetItemTooltip("Select a scene to push onto stack");
    }

    ImGui::SameLine();
    if (ImGui::Button("Replace") && hasSelection)
    {
        auto it = service.scenes_.begin();
        std::advance(it, selectedSceneIndex_);
        service.Replace(it->first);
    }
    if (!hasSelection)
    {
        ImGui::SetItemTooltip("Select a scene to replace top of stack");
    }

    ImGui::SameLine();
    if (ImGui::Button("Pop"))
    {
        service.Pop();
    }
    if (service.IsEmpty())
    {
        ImGui::SetItemTooltip("Stack is empty");
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        service.Clear();
    }

    ImGui::Separator();

    // Scene Registry Table
    ImGui::Text("Scene Registry:");
    if (ImGui::BeginTable("SceneTable", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0, -80)))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("XML Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("In Stack", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        int index = 0;
        for (const auto& [name, sceneData] : service.scenes_)
        {
            ImGui::TableNextRow();

            // Name column with selection
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(name.c_str(), selectedSceneIndex_ == index, ImGuiSelectableFlags_SpanAllColumns))
            {
                selectedSceneIndex_ = index;
            }

            // XML Path column
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", sceneData.xmlPath.c_str());

            // In Stack column
            ImGui::TableSetColumnIndex(2);
            bool inStack = false;
            for (Scene* s : service.sceneStack_)
            {
                if (s == sceneData.scene)
                {
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
    if (ImGui::BeginTable("StackTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, 0)))
    {
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Scene Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // Display stack from top to bottom (reverse order)
        const auto& stack = service.sceneStack_;
        for (int i = (int)stack.size() - 1; i >= 0; --i)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (i == (int)stack.size() - 1)
            {
                ImGui::Text("TOP");
            }
            else if (i == 0)
            {
                ImGui::Text("BOTTOM");
            }
            else
            {
                ImGui::Text("%d", i);
            }

            ImGui::TableSetColumnIndex(1);
            // Find scene name
            std::string sceneName = "Unknown";
            for (const auto& [name, data] : service.scenes_)
            {
                if (data.scene == stack[i])
                {
                    sceneName = name;
                    break;
                }
            }
            ImGui::Text("%s", sceneName.c_str());
        }

        if (stack.empty())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("-");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("(empty)");
        }

        ImGui::EndTable();
    }
}

void SceneInspector::DrawCurrentScenePanel(SceneService& service)
{
    Scene* topScene = service.GetTopScene();

    if (topScene)
    {
        std::string topSceneName = "Unknown";
        for (const auto& [name, data] : service.scenes_)
        {
            if (data.scene == topScene)
            {
                topSceneName = name;
                break;
            }
        }

        ImGui::Text("Top Scene: %s", topSceneName.c_str());
        ImGui::Text("Stack Size: %zu", service.GetStackSize());

        ImGui::Separator();

        // Systems drawer
        DrawSystemsDrawer(service);
    }
    else
    {
        ImGui::Text("No scenes in stack");
        ImGui::Text("Select a scene from the left panel and click 'Push' to begin.");
    }
}

void SceneInspector::DrawSystemsDrawer(SceneService& service)
{
    static bool systemsOpen = true;
    if (ImGui::CollapsingHeader("Systems", systemsOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0))
    {
        Scene* topScene = service.GetTopScene();
        if (!topScene)
        {
            ImGui::Text("No active scene");
            return;
        }

        const auto& systems = topScene->GetSystems();
        if (systems.empty())
        {
            ImGui::Text("No systems in current scene");
            return;
        }

        if (ImGui::BeginTable("SystemsTable", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_ScrollY,
                              ImVec2(0, 150)))
        {
            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("System Type", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < systems.size(); ++i)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", i);

                ImGui::TableSetColumnIndex(1);
                const std::type_info& typeInfo = typeid(*systems[i]);
                std::string systemName = typeInfo.name();

                size_t lastColon = systemName.find_last_of(':');
                if (lastColon != std::string::npos && lastColon < systemName.length() - 1)
                {
                    systemName = systemName.substr(lastColon + 1);
                }

                ImGui::Text("%s", systemName.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("Active");
            }

            ImGui::EndTable();
        }

        ImGui::Text("Total Systems: %zu", systems.size());
    }
}

void SceneInspector::DrawAssetsDrawer(SceneService& service)
{
    // Assets drawer removed - GetAssets() no longer exists
    // Assets are now managed globally through AssetService
}

} // namespace Elysium

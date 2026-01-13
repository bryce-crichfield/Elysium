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
    ImGui::SameLine(leftPanelWidth_ + 10); // Position right side header
    ImGui::Text("Current Scene");

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

    // Handle dragging - only use mouse delta when already dragging
    if (ImGui::IsItemActive())
    {
        if (!isDraggingSplitter_)
        {
            // First frame of dragging - don't apply delta yet, just mark as dragging
            isDraggingSplitter_ = true;
        }
        else
        {
            // Subsequent frames - apply mouse delta
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

    // Right panel - Current Scene Panel
    ImGui::BeginChild("CurrentScenePanel", ImVec2(0, 0), true);
    DrawCurrentScenePanel(service);
    ImGui::EndChild();
}

void SceneInspector::DrawScenesPanel(SceneService& service)
{
    // Scene Management Buttons
    if (ImGui::Button("Create"))
    {
        // TODO: Implement scene creation
    }
    ImGui::SameLine();

    bool hasSelection = selectedSceneIndex_ >= 0 && selectedSceneIndex_ < service.scenes_.size();

    if (ImGui::Button("Load") && hasSelection)
    {
        auto it = service.scenes_.begin();
        std::advance(it, selectedSceneIndex_);
        service.QueueScene(it->first);
    }
    if (!hasSelection)
    {
        ImGui::SetItemTooltip("Select a scene to load");
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove") && hasSelection)
    {
        // TODO: Implement scene removal
    }
    if (!hasSelection)
    {
        ImGui::SetItemTooltip("Select a scene to remove");
    }

    ImGui::Separator();

    // Scene Registry Table
    ImGui::Text("Scene Registry:");
    if (ImGui::BeginTable("SceneTable", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0, -80)))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("XML Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        int index = 0;
        for (const auto &[name, sceneData] : service.scenes_)
        {
            ImGui::TableNextRow();

            // Name column with selection
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(name.c_str(), selectedSceneIndex_ == index, ImGuiSelectableFlags_SpanAllColumns))
            {
                selectedSceneIndex_ = index;
            }

            // Status column
            ImGui::TableSetColumnIndex(1);
            std::string statusText = PrintSceneStatus(sceneData.status);
            ImGui::Text("%s", statusText.c_str());

            // XML Path column
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", sceneData.xmlPath.c_str());

            // Assets column
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%zu", sceneData.assets.size());

            index++;
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    // Scene Queue Table
    ImGui::Text("Scene Queue:");
    if (ImGui::BeginTable("QueueTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, 0)))
    {
        ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Scene Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        std::queue<std::string> tempQueue = service.sceneQueue_;
        int order = 1;
        while (!tempQueue.empty())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", order++);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", tempQueue.front().c_str());
            ImGui::TableSetColumnIndex(2);
            auto it = service.scenes_.find(tempQueue.front());
            if (it != service.scenes_.end())
            {
                std::string statusText = PrintSceneStatus(it->second.status);
                ImGui::Text("%s", statusText.c_str());
            }
            tempQueue.pop();
        }
        ImGui::EndTable();
    }
}

void SceneInspector::DrawCurrentScenePanel(SceneService& service)
{
    // Active scene info and controls
    if (service.activeScene_)
    {
        std::string activeSceneName = "Unknown";
        for (const auto &[name, data] : service.scenes_)
        {
            if (data.scene == service.activeScene_)
            {
                activeSceneName = name;
                break;
            }
        }

        ImGui::Text("Active Scene: %s", activeSceneName.c_str());

        // Play/Pause/Step controls
        SceneState currentState = service.transitions_.GetCurrentState();
        if (currentState == SceneState::ACTIVE_RUNNING)
        {
            if (ImGui::Button("Pause"))
            {
                service.transitions_.stateMachine_.TransitionTo(SceneStateToString(SceneState::ACTIVE_PAUSED));
            }
        }
        else if (currentState == SceneState::ACTIVE_PAUSED)
        {
            if (ImGui::Button("Play"))
            {
                service.transitions_.stateMachine_.TransitionTo(SceneStateToString(SceneState::ACTIVE_RUNNING));
            }
            ImGui::SameLine();

            // Timeout configuration
            ImGui::PushItemWidth(80);
            float timeoutDuration = service.transitions_.GetTransitionDuration();
            if (ImGui::InputFloat("##timeout", &timeoutDuration, 10.0f, 100.0f, "%.0f"))
            {
                service.transitions_.SetTimeoutDuration(timeoutDuration);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Timeout duration in milliseconds\nRun for this duration then pause");
            }
            ImGui::PopItemWidth();
            if (timeoutDuration < 1.0f)
            {
                service.transitions_.SetTimeoutDuration(1.0f);
            }

            ImGui::SameLine();
            ImGui::Text("ms");
            ImGui::SameLine();

            if (ImGui::Button("Timeout"))
            {
                service.transitions_.StartTimeout();
            }

            // Show timeout progress if currently timing out
            // Note: We'd need to expose isTimingOut_ via a getter in SceneTransitionController
        }

        ImGui::Text("State: %s", SceneStateToString(currentState));
        if (service.transitions_.IsTransitioning())
        {
            ImGui::Text("Progress: %.2f", service.transitions_.GetTransitionProgress());
        }

        ImGui::Separator();

        // Systems drawer
        DrawSystemsDrawer(service);

        ImGui::Separator();

        // Assets drawer
        DrawAssetsDrawer(service);
    }
    else
    {
        ImGui::Text("No active scene");
        ImGui::Text("Select a scene from the left panel and click 'Load' to begin.");
    }
}

void SceneInspector::DrawSystemsDrawer(SceneService& service)
{
    static bool systemsOpen = true;
    if (ImGui::CollapsingHeader("Systems", systemsOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0))
    {
        if (!service.activeScene_)
        {
            ImGui::Text("No active scene");
            return;
        }

        const auto &systems = service.activeScene_->GetSystems();
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
                // Get system type name using typeid - this is basic but functional
                const std::type_info &typeInfo = typeid(*systems[i]);
                std::string systemName = typeInfo.name();

                // Clean up the type name (remove namespace prefixes if present)
                size_t lastColon = systemName.find_last_of(':');
                if (lastColon != std::string::npos && lastColon < systemName.length() - 1)
                {
                    systemName = systemName.substr(lastColon + 1);
                }

                ImGui::Text("%s", systemName.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("Active"); // All systems in the list are active
            }

            ImGui::EndTable();
        }

        ImGui::Text("Total Systems: %zu", systems.size());
    }
}

void SceneInspector::DrawAssetsDrawer(SceneService& service)
{
    static bool assetsOpen = true;
    if (ImGui::CollapsingHeader("Assets", assetsOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0))
    {
        if (!service.activeScene_)
        {
            ImGui::Text("No active scene");
            return;
        }

        // Get assets from the current scene
        std::vector<Asset> sceneAssets = service.activeScene_->GetAssets();

        if (sceneAssets.empty())
        {
            ImGui::Text("No assets declared by current scene");
            return;
        }

        if (ImGui::BeginTable("AssetsTable", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_ScrollY,
                              ImVec2(0, 150)))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (const auto &asset : sceneAssets)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", asset.GetName().c_str());

                ImGui::TableSetColumnIndex(1);
                // Convert AssetType to string
                std::string typeStr;
                switch (asset.GetType())
                {
                case AssetType::TEXTURE:
                    typeStr = "TEXTURE";
                    break;
                case AssetType::SOUND:
                    typeStr = "SOUND";
                    break;
                case AssetType::MUSIC:
                    typeStr = "MUSIC";
                    break;
                case AssetType::FONT:
                    typeStr = "FONT";
                    break;
                case AssetType::MODEL:
                    typeStr = "MODEL";
                    break;
                case AssetType::SHADER:
                    typeStr = "SHADER";
                    break;
                case AssetType::SPRITE:
                    typeStr = "SPRITE";
                    break;
                default:
                    typeStr = "UNKNOWN";
                    break;
                }
                ImGui::Text("%s", typeStr.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", asset.GetPath().c_str());

                ImGui::TableSetColumnIndex(3);
                // For now, just show "Loaded" - in the future this could check AssetService
                ImGui::Text("Loaded");
            }

            ImGui::EndTable();
        }

        ImGui::Text("Total Assets: %zu", sceneAssets.size());
    }
}

} // namespace Elysium

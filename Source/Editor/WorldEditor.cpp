#include "WorldEditor.h"
#include <algorithm>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Services/WorldService.h"
#include "imgui.h"

namespace Elysium {

using namespace Services;

WorldEditor::WorldEditor() : Editor("World Editor") {}

void WorldEditor::Draw(Application& app) {
    Profile;

    auto& service = app.GetService<WorldService>();
    auto* world = service.GetWorld();

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(name_.c_str(), &isVisible_, ImGuiWindowFlags_NoCollapse)) {
        if (!world) {
            ImGui::Text("No World Loaded");
            ImGui::End();
            return;
        }

        // Left side header
        ImGui::Text("Entities");
        ImGui::SameLine(leftPanelWidth_ + 10);
        ImGui::Text("Inspector");

        // Left panel - Entity List
        ImGui::BeginChild("EntityPanel", ImVec2(leftPanelWidth_, 0), true);
        DrawEntityToolbar(service);

        // Scrollable entity table
        ImGui::BeginChild("EntityList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        DrawEntityList(service);
        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::SameLine();

        // Splitter
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::Button("##splitter", ImVec2(4.0f, -1));

        if (ImGui::IsItemActive()) {
            if (!isDraggingSplitter_) {
                isDraggingSplitter_ = true;
            } else {
                leftPanelWidth_ += ImGui::GetIO().MouseDelta.x;
                if (leftPanelWidth_ < 200.0f)
                    leftPanelWidth_ = 200.0f;
                if (leftPanelWidth_ > ImGui::GetWindowWidth() - 200.0f)
                    leftPanelWidth_ = ImGui::GetWindowWidth() - 200.0f;
            }
        } else {
            isDraggingSplitter_ = false;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();

        // Right panel - Inspector
        ImGui::BeginChild("Inspector", ImVec2(0, 0), true);
        DrawInspectorToolbar(service);
        DrawInspectorPanel(service);
        ImGui::EndChild();
    }
    ImGui::End();
}

void WorldEditor::DrawEntityToolbar(WorldService& service) {
    auto* world = service.GetWorld();
    const auto& componentPlaceholders = service.GetComponentPlaceholders();

    ImGui::Text("Search: ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    static char searchBuffer[256] = "";
    if (ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer))) {
        searchFilter_ = std::string(searchBuffer);
    }
    ImGui::Separator();

    // Filter panel
    if (ImGui::CollapsingHeader("Filters", filtersCollapsed_ ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
        // Clear button at the top
        if (ImGui::Button("Clear All Filters")) {
            filters_.clear();
        }

        // Filter table
        if (!filters_.empty() && ImGui::BeginTable("Filters", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("NOT", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Op", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < filters_.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                // Negation checkbox
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("##negate", &filters_[i].negate);

                // Component dropdown
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1);
                std::vector<const char*> componentNames;
                int currentSelection = -1;
                for (size_t j = 0; j < componentPlaceholders.size(); ++j) {
                    componentNames.push_back(componentPlaceholders[j].name.c_str());
                    if (componentPlaceholders[j].name == filters_[i].componentName) {
                        currentSelection = static_cast<int>(j);
                    }
                }

                if (ImGui::Combo("##component", &currentSelection, componentNames.data(), static_cast<int>(componentNames.size()))) {
                    if (currentSelection >= 0) {
                        filters_[i].componentName = componentPlaceholders[currentSelection].name;
                        filters_[i].predicate = componentPlaceholders[currentSelection].hasComponentFunc;
                    }
                }

                // Operator dropdown (skip for first filter)
                ImGui::TableSetColumnIndex(2);
                if (i > 0) {
                    const char* operators[] = {"AND", "OR"};
                    int currentOp = static_cast<int>(filters_[i].logicalOperator);
                    if (ImGui::Combo("##operator", &currentOp, operators, IM_ARRAYSIZE(operators))) {
                        filters_[i].logicalOperator = static_cast<FilterLogicalOperator>(currentOp);
                    }
                } else {
                    ImGui::Text("-");
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        // Add/Remove buttons at the bottom
        if (ImGui::Button("Add Filter")) {
            filters_.emplace_back();
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Filter") && !filters_.empty()) {
            filters_.pop_back();
        }
    }

    ImGui::Separator();

    // Entity management buttons
    Entity selectedEntity = service.GetSelectedEntity();
    bool canDeselect = selectedEntity != INVALID_ENTITY;
    if (!canDeselect) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }
    if (ImGui::Button("Deselect")) {
        if (canDeselect)
            service.SetSelectedEntity(INVALID_ENTITY);
    }
    if (!canDeselect) {
        ImGui::PopStyleVar();
    }
}

void WorldEditor::DrawEntityList(WorldService& service) {
    auto* world = service.GetWorld();
    Entity selectedEntity = service.GetSelectedEntity();

    if (ImGui::BeginTable("Entities", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const auto& livingEntities = world->GetEntitiesWithComponents<>();

        for (Entity entity : livingEntities) {
            bool shouldDraw = true;
            if (!filters_.empty()) {
                // Evaluate first filter (only if it has a valid predicate)
                if (filters_[0].predicate) {
                    shouldDraw = filters_[0].Evaluate(entity, world);
                }

                // Evaluate remaining filters in order
                for (size_t i = 1; i < filters_.size(); ++i) {
                    if (filters_[i].predicate) {
                        bool filterResult = filters_[i].Evaluate(entity, world);

                        if (filters_[i].logicalOperator == FilterLogicalOperator::AND) {
                            shouldDraw = shouldDraw && filterResult;
                        } else {
                            shouldDraw = shouldDraw || filterResult;
                        }
                    }
                }
            }

            if (!shouldDraw)
                continue;

            std::string entityName = world->GetEntityName(entity);
            if (entityName.empty()) {
                entityName = "Entity " + std::to_string(entity);
            }

            if (!searchFilter_.empty()) {
                std::string lowerName = entityName;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::string lowerFilter = searchFilter_;
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

                if (lowerName.find(lowerFilter) == std::string::npos) {
                    continue;
                }
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            bool isSelected = (entity == selectedEntity);
            if (ImGui::Selectable(std::to_string(entity).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                service.SetSelectedEntity(entity);
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem(("EntityContextMenu_" + std::to_string(entity)).c_str())) {
                ImGui::Text("Entity %zu", entity);
                ImGui::Separator();

                if (ImGui::MenuItem("Edit")) {
                    service.SetSelectedEntity(entity);
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::MenuItem("Clone")) {
                    Entity clonedEntity = world->CloneEntity(entity);
                    service.SetSelectedEntity(clonedEntity);
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::MenuItem("Delete")) {
                    world->DestroyEntity(entity);
                    if (selectedEntity == entity) {
                        service.SetSelectedEntity(INVALID_ENTITY);
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", entityName.c_str());
        }

        ImGui::EndTable();

        // Right-click context menu on empty space
        if (ImGui::BeginPopupContextWindow("EntityListContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Entity")) {
                Entity newEntity = world->CreateEntity();
                service.SetSelectedEntity(newEntity);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void WorldEditor::DrawInspectorToolbar(WorldService& service) {
    auto* world = service.GetWorld();
    Entity selectedEntity = service.GetSelectedEntity();
    const auto& componentPlaceholders = service.GetComponentPlaceholders();

    if (selectedEntity != INVALID_ENTITY) {
        std::string entityName = world->GetEntityName(selectedEntity);
        if (entityName.empty()) {
            entityName = "Entity " + std::to_string(selectedEntity);
        }

        ImGui::Text("Entity ID: %zu", selectedEntity);

        // Entity name editing
        ImGui::Text("Name:");
        ImGui::SameLine();
        static char nameBuffer[256];

        static Entity lastEditedEntity = INVALID_ENTITY;
        if (lastEditedEntity != selectedEntity) {
            strncpy(nameBuffer, entityName.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            lastEditedEntity = selectedEntity;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##EntityName", nameBuffer, sizeof(nameBuffer))) {
            if (world->HasComponent<NameComponent>(selectedEntity)) {
                world->GetComponent<NameComponent>(selectedEntity).name = nameBuffer;
            } else {
                world->AddComponent(selectedEntity, NameComponent(nameBuffer));
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            for (const auto& placeholder : componentPlaceholders) {
                if (!placeholder.hasComponentFunc(selectedEntity, world)) {
                    if (ImGui::MenuItem(placeholder.name.c_str())) {
                        placeholder.addComponentFunc(selectedEntity, world);
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndPopup();
        }
    } else {
        ImGui::Text("No entity selected");
    }
}

void WorldEditor::DrawInspectorPanel(WorldService& service) {
    Entity selectedEntity = service.GetSelectedEntity();
    if (selectedEntity == INVALID_ENTITY)
        return;

    auto* world = service.GetWorld();
    const auto& componentPlaceholders = service.GetComponentPlaceholders();

    componentToDelete_ = "";

    for (size_t i = 0; i < componentPlaceholders.size(); ++i) {
        if (componentPlaceholders[i].hasComponentFunc(selectedEntity, world)) {
            DrawComponentPanel(service, i);
        }
    }

    // Process deletion after the loop
    if (!componentToDelete_.empty()) {
        for (const auto& placeholder : componentPlaceholders) {
            if (placeholder.name == componentToDelete_) {
                placeholder.removeComponentFunc(selectedEntity, world);
                break;
            }
        }
        componentToDelete_ = "";
    }
}

void WorldEditor::DrawComponentPanel(WorldService& service, size_t placeholderIndex) {
    Entity selectedEntity = service.GetSelectedEntity();
    auto* world = service.GetWorld();
    const auto& placeholder = service.GetComponentPlaceholders()[placeholderIndex];

    ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

    bool nodeOpen = ImGui::TreeNodeEx(placeholder.name.c_str(), treeFlags);

    ImGui::PushID(placeholder.name.c_str());
    ImGui::PopID();

    if (nodeOpen) {
        if (ImGui::SmallButton("Delete")) {
            componentToDelete_ = placeholder.name;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) {
            placeholder.resetComponentFunc(selectedEntity, world);
        }

        ImGui::Separator();

        placeholder.drawFunc(selectedEntity, world);
        ImGui::TreePop();
    }
}



}  // namespace Elysium

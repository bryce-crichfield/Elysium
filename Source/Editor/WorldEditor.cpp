#include "WorldEditor.h"
#include <algorithm>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Services/EditorService.h"
#include "Services/ScriptService.h"
#include "imgui.h"
#include "Components/NameComponent.h"
#include "Services/LogService.h"

namespace Elysium {

using namespace Services;

WorldEditor::WorldEditor() : Editor("World Editor") {}

void WorldEditor::Draw(Application& app) {
    Profile;

    auto& service = app.GetService<EditorService>();
    auto* world = service.GetWorld();

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(name_.c_str(), nullptr, ImGuiWindowFlags_NoCollapse)) {
        if (!world) {
            ImGui::Text("No World Loaded");
            ImGui::End();
            return;
        }

        // Left side header
        ImGui::Text("Entities");
        ImGui::SameLine(leftPanelWidth_ + 10);
        ImGui::Text("Inspector");

        // 1. Entity List (Top half)
        float topPanelHeight = ImGui::GetContentRegionAvail().y * 0.4f; // Use 40% height
        ImGui::BeginChild("EntityPanel", ImVec2(0, topPanelHeight), true);
            DrawEntityToolbar(service);
            if (showHierarchyView_)
                DrawHierarchyTree(service);
            else
                DrawEntityList(service);
        ImGui::EndChild();

        // 2. Horizontal Splitter (Vertical movement)
        ImGui::Button("##splitter", ImVec2(-1, 4.0f)); 
        // Logic to adjust topPanelHeight based on MouseDelta.y...

        // 3. Inspector (Bottom half)
        ImGui::BeginChild("Inspector", ImVec2(0, 0), true);
            DrawInspectorToolbar(service);
            DrawInspectorPanel(service);
        ImGui::EndChild();
    }
    ImGui::End();
}

void WorldEditor::DrawEntityToolbar(EditorService& service) {
    auto* world = service.GetWorld();
    const auto& componentPlaceholders = service.GetComponentPlaceholders();

    ImGui::Text("Search: ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    static char searchBuffer[256] = "";
    if (ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer))) {
    }
    ImGui::Separator();

    // Filter panel
    if (ImGui::CollapsingHeader("Lua Filter", ImGuiTreeNodeFlags_DefaultOpen)) {
        static char luaBuffer[1024] = "function filter(e)\n  return true\nend";
        
        // Use a tall input box for the script
        ImGui::InputTextMultiline("##LuaFilter", luaBuffer, sizeof(luaBuffer), 
            ImVec2(-1, ImGui::GetTextLineHeight() * 5), // 5 lines tall
            ImGuiInputTextFlags_AllowTabInput);

        if (ImGui::Button("Save", ImVec2(-1, 0))) {
            // 1. 'template' is a reserved keyword in C++. Changed to 'scriptTemplate'.
            // 2. Used R"(...)" for a raw string literal to handle multiple lines.
            const char* scriptTemplate = R"(
                %s

                local entities = GetEntities()
                local result = {}
                for i, e in ipairs(entities) do
                    if filter(e) then
                        table.insert(result, e)
                    end
                end

                return result
            )";

            // 3. Format the template with the user's buffer (luaBuffer)
            char finalScript[2048]; 
            snprintf(finalScript, sizeof(finalScript), scriptTemplate, luaBuffer);

            auto& scripts = Application::GetInstance().GetService<ScriptService>();
            // 4. Execute the formatted string, not just the raw buffer
            auto result = scripts.ExecuteString(finalScript);
            filteredEntities_.clear();
            if (result.valid()) {
                sol::table entityTable = result;

                // print the ids of the matching entities
                for (auto& kv : entityTable) {
                    sol::object key = kv.first;
                    sol::object val = kv.second;

                    if (val.is<Entity>()) {
                        Entity e = val.as<Entity>();
                        filteredEntities_.push_back(e);
                    }
                }
            } else {
                // Handle Lua error
                sol::error err = result;
                LOG_ERRORF("WorldEditor", "Lua Error in filter script: %s", err.what());
            }
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Expects: function(entity) -> bool");
        }
    }

    ImGui::Separator();

    // View toggle
    if (ImGui::SmallButton(showHierarchyView_ ? "View: Hierarchy" : "View: Flat")) {
        showHierarchyView_ = !showHierarchyView_;
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

void WorldEditor::DrawEntityList(EditorService& service) {
    auto* world = service.GetWorld();
    Entity selectedEntity = service.GetSelectedEntity();

    if (ImGui::BeginTable("Entities", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const auto& livingEntities = world->GetEntitiesWithComponents<>();

        for (Entity entity : livingEntities) {
            if (!filteredEntities_.empty()) {
                // If filtering is active, skip entities not in the filtered list
                if (std::find(filteredEntities_.begin(), filteredEntities_.end(), entity) == filteredEntities_.end()) {
                    continue;
                }
            }

            std::string entityName = world->GetEntityName(entity);
            if (entityName.empty()) {
                entityName = "Entity " + std::to_string(entity);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            bool isSelected = (entity == selectedEntity);
            if (ImGui::Selectable(std::to_string(entity).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                service.SetSelectedEntity(entity);
            }

            DrawEntityContextMenu(service, entity);

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

void WorldEditor::DrawInspectorToolbar(EditorService& service) {
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

void WorldEditor::DrawInspectorPanel(EditorService& service) {
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

void WorldEditor::DrawComponentPanel(EditorService& service, size_t placeholderIndex) {
    Entity selectedEntity = service.GetSelectedEntity();
    auto* world = service.GetWorld();
    const auto& placeholder = service.GetComponentPlaceholders()[placeholderIndex];

    ImGuiTreeNodeFlags treeFlags =  ImGuiTreeNodeFlags_Framed;

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



void WorldEditor::DrawEntityContextMenu(EditorService& service, Entity entity) {
    auto* world = service.GetWorld();
    Entity selectedEntity = service.GetSelectedEntity();

    if (ImGui::BeginPopupContextItem(("EntityContextMenu_" + std::to_string(entity)).c_str())) {
        ImGui::Text("Entity %zu", entity);
        ImGui::Separator();

        if (ImGui::MenuItem("Edit")) {
            service.SetSelectedEntity(entity);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Clone")) {
            Entity cloned = world->CloneEntity(entity);
            service.SetSelectedEntity(cloned);
            ImGui::CloseCurrentPopup();
        }

        Entity parent = world->GetParent(entity);
        if (parent != INVALID_ENTITY) {
            if (ImGui::MenuItem("Detach from Parent")) {
                world->RemoveChild(parent, entity);
                ImGui::CloseCurrentPopup();
            }
        }

        if (ImGui::MenuItem("Delete")) {
            world->DestroyEntity(entity);
            if (selectedEntity == entity)
                service.SetSelectedEntity(INVALID_ENTITY);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void WorldEditor::DrawInsertionZone(EditorService& service, Entity parent, Entity beforeSibling) {
    auto* world = service.GetWorld();

    // Two-level PushID gives each zone a unique scope without string allocation.
    ImGui::PushID((int)parent);
    ImGui::PushID(beforeSibling == INVALID_ENTITY ? -1 : (int)beforeSibling);

    float zoneH = 4.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float zoneW = ImGui::GetContentRegionAvail().x;

    ImGui::InvisibleButton("##zone", ImVec2(zoneW > 0 ? zoneW : 1.0f, 2.0f));

    if (ImGui::BeginDragDropTarget()) {
        // Visual insertion line while hovering.
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(origin.x, origin.y + zoneH * 0.5f),
            ImVec2(origin.x + zoneW, origin.y + zoneH * 0.5f),
            IM_COL32(100, 200, 255, 220), 2.0f);

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG")) {
            Entity dragged = *(const Entity*)payload->Data;

            // Prevent dropping an entity onto itself or creating a hierarchy cycle.
            bool isSelf    = (dragged == parent);
            bool wouldLoop = (parent != INVALID_ENTITY) && world->IsAncestorOf(dragged, parent);

            if (!isSelf && !wouldLoop) {
                // Detach from current parent if it has one.
                Entity oldParent = world->GetParent(dragged);
                if (oldParent != INVALID_ENTITY)
                    world->RemoveChild(oldParent, dragged);

                if (parent == INVALID_ENTITY) {
                    // Root-level drop: entity is already detached above; just reorder.
                    if (beforeSibling != INVALID_ENTITY)
                        world->MoveEntityBefore(dragged, beforeSibling);
                    // beforeSibling == INVALID_ENTITY means "end of list" — no reorder needed.
                } else if (beforeSibling == INVALID_ENTITY) {
                    // Append as last child of parent.
                    world->AddChild(parent, dragged);
                    // livingEntities order: move dragged after parent so it draws on top.
                    world->MoveEntityAfter(dragged, parent);
                } else {
                    // Insert as sibling immediately before beforeSibling.
                    world->InsertChildBefore(parent, dragged, beforeSibling);
                    world->MoveEntityBefore(dragged, beforeSibling);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
    ImGui::PopID();
}

void WorldEditor::DrawHierarchyNode(EditorService& service, Entity entity) {
    auto* world = service.GetWorld();
    Entity selectedEntity = service.GetSelectedEntity();

    // Copy children now — insertion zones can mutate childrenMap_ mid-frame.
    std::vector<Entity> children(world->GetChildren(entity));
    bool hasChildren = !children.empty();

    std::string label = world->GetEntityName(entity);
    if (label.empty())
        label = "Entity " + std::to_string(entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (entity == selectedEntity)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool open = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s  [%zu]", label.c_str(), entity);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        service.SetSelectedEntity(entity);

    // Drag source: let this node be dragged.
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("ENTITY_DRAG", &entity, sizeof(Entity));
        ImGui::Text("Move: %s", label.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target on the node itself: make dragged entity a child of this one.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG")) {
            Entity dragged = *(const Entity*)payload->Data;
            if (dragged != entity
                && world->GetParent(dragged) != entity
                && !world->IsAncestorOf(dragged, entity))
            {
                Entity oldParent = world->GetParent(dragged);
                if (oldParent != INVALID_ENTITY)
                    world->RemoveChild(oldParent, dragged);
                world->AddChild(entity, dragged);
                // Draw dragged after this entity so parent renders before child.
                world->MoveEntityAfter(dragged, entity);
            }
        }
        ImGui::EndDragDropTarget();
    }

    DrawEntityContextMenu(service, entity);

    if (hasChildren && open) {
        for (Entity child : children) {
            DrawInsertionZone(service, entity, child);
            DrawHierarchyNode(service, child);
        }
        DrawInsertionZone(service, entity, INVALID_ENTITY);
        ImGui::TreePop();
    }
}

void WorldEditor::DrawHierarchyTree(EditorService& service) {
    auto* world = service.GetWorld();

    // Build a snapshot of root entities so insertion-zone drops don't invalidate iteration.
    std::vector<Entity> roots;
    for (Entity entity : world->GetLivingEntities()) {
        if (world->GetParent(entity) != INVALID_ENTITY)
            continue;
        if (!filteredEntities_.empty()) {
            if (std::find(filteredEntities_.begin(), filteredEntities_.end(), entity) == filteredEntities_.end())
                continue;
        }
        roots.push_back(entity);
    }

    for (Entity root : roots) {
        // Insertion zone before each root: drop here to reorder at root level or unparent.
        DrawInsertionZone(service, INVALID_ENTITY, root);
        DrawHierarchyNode(service, root);
    }
    // Zone after the last root.
    DrawInsertionZone(service, INVALID_ENTITY, INVALID_ENTITY);

    // Right-click on empty space to create entity
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu",
                                       ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create Entity")) {
            Entity e = world->CreateEntity();
            service.SetSelectedEntity(e);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace Elysium

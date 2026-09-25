#include "HierarchyEditor.h"
#include <algorithm>
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Editor/Widgets.h"
#include "Interfaces/IEditorService.h"
#include "Interfaces/IScriptService.h"

namespace Elysium {

using namespace Services;

namespace {
constexpr const char* kEntityDragPayload = "ENTITY_DRAG";

// The entity dropped on the current drag target this frame, if any.
Entity AcceptEntityDrop(ImGuiDragDropFlags flags = 0) {
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityDragPayload, flags);
    return payload ? *(const Entity*)payload->Data : INVALID_ENTITY;
}

void Detach(World& world, Entity entity) {
    const Entity parent = world.GetParent(entity);
    if (parent != INVALID_ENTITY) world.RemoveChild(parent, entity);
}

// Makes `child` the last child of `parent`, drawn after it so the parent renders first.
void AppendChild(World& world, Entity parent, Entity child) {
    Detach(world, child);
    world.AddChild(parent, child);
    world.MoveEntityAfter(child, parent);
}

void CreateAndSelect(IEditorService& service) { service.SelectEntity(service.GetWorld()->CreateEntity()); }
}  // namespace

HierarchyEditor::HierarchyEditor(ServiceLocator& services) : Editor(services, Title) {}

void HierarchyEditor::Draw() {
    Profile;

    auto& service = services_.Get<IEditorService>();

    if (BeginWindow()) {
        if (!service.GetWorld()) {
            EmptyState("No world loaded");
        } else {
            DrawToolbar(service);
            if (showLuaFilter_) DrawLuaFilter();

            ImGui::BeginChild("Entities", ImVec2(0, 0), ImGuiChildFlags_None);
            // A name search flattens the tree: matches can sit under collapsed parents.
            if (showHierarchyView_ && searchBuffer_[0] == '\0')
                DrawHierarchyTree(service);
            else
                DrawEntityList(service);

            // Clicking empty space deselects; right-clicking it offers entity creation.
            if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                service.ClearSelection();
            if (ImGui::BeginPopupContextWindow("HierarchyContextMenu",
                                               ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                DrawCreateEntityMenu(service);
                ImGui::EndPopup();
            }
            ImGui::EndChild();
            openRequest_.reset();
        }
    }
    EndWindow();
}

void HierarchyEditor::DrawToolbar(IEditorService& service) {
    const char* viewIcon = showHierarchyView_ ? ICON_FA_LIST : ICON_FA_SITEMAP;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float buttonsWidth = ButtonWidth(ICON_FA_PLUS) + ButtonWidth(viewIcon) +
                               ButtonWidth(ICON_FA_FILTER) + spacing * 3.0f + ExpandCollapseWidth();

    SearchField("##Search", searchBuffer_, sizeof(searchBuffer_), -buttonsWidth);

    ImGui::SameLine();
    if (IconButton(ICON_FA_PLUS, "Create entity")) CreateAndSelect(service);
    ImGui::SameLine();
    if (IconButton(viewIcon, showHierarchyView_ ? "Show as flat list" : "Show as hierarchy")) {
        showHierarchyView_ = !showHierarchyView_;
    }
    ImGui::SameLine();
    if (luaFilterActive_) ImGui::PushStyleColor(ImGuiCol_Text, Palette::Accent);
    if (IconButton(ICON_FA_FILTER, "Lua filter")) showLuaFilter_ = !showLuaFilter_;
    if (luaFilterActive_) ImGui::PopStyleColor();

    // Only the tree has anything to expand; a search or the list view is flat.
    ImGui::SameLine();
    ImGui::BeginDisabled(!showHierarchyView_ || searchBuffer_[0] != '\0');
    ExpandCollapseButtons(openRequest_);
    ImGui::EndDisabled();
}

void HierarchyEditor::DrawLuaFilter() {
    ImGui::InputTextMultiline("##LuaFilter", luaFilterBuffer_, sizeof(luaFilterBuffer_),
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5), ImGuiInputTextFlags_AllowTabInput);
    ItemTooltip("Expects: function(entity) -> bool");

    const float half = SharedButtonWidth(2);
    if (PrimaryButton("Apply", ImVec2(half, 0))) {
        filteredEntities_ = services_.Get<IScriptService>().FilterEntities(luaFilterBuffer_);
        luaFilterActive_ = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!luaFilterActive_);
    if (ImGui::Button("Clear", ImVec2(half, 0))) {
        filteredEntities_.clear();
        luaFilterActive_ = false;
    }
    ImGui::EndDisabled();
    ImGui::Separator();
}

bool HierarchyEditor::PassesFilters(const World& world, Entity entity) const {
    if (luaFilterActive_ &&
        std::find(filteredEntities_.begin(), filteredEntities_.end(), entity) == filteredEntities_.end())
        return false;
    return MatchesSearch(EntityLabel(world.GetEntityName(entity), entity), searchBuffer_);
}

void HierarchyEditor::DrawEntityList(IEditorService& service) {
    auto* world = service.GetWorld();

    if (!ImGui::BeginTable("Entities", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX)) return;
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, Theme::IdColumnWidth);

    for (Entity entity : world->GetLivingEntities()) {
        if (!PassesFilters(*world, entity)) continue;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushID((int)entity);
        const std::string label = EntityLabel(world->GetEntityName(entity), entity);
        if (ImGui::Selectable(label.c_str(), service.IsSelected(entity), ImGuiSelectableFlags_SpanAllColumns)) {
            service.SelectEntity(entity);
        }
        DrawEntityContextMenu(service, entity);
        ImGui::PopID();

        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("%zu", entity);
    }
    ImGui::EndTable();
}

void HierarchyEditor::DrawCreateEntityMenu(IEditorService& service) {
    if (ImGui::MenuItem(ICON_FA_PLUS "  Create Entity")) CreateAndSelect(service);
}

void HierarchyEditor::DrawEntityContextMenu(IEditorService& service, Entity entity) {
    auto* world = service.GetWorld();

    if (!ImGui::BeginPopupContextItem("EntityContextMenu")) return;

    ImGui::TextDisabled("%s", EntityLabel(world->GetEntityName(entity), entity).c_str());
    ImGui::Separator();

    if (ImGui::MenuItem(ICON_FA_COPY "  Duplicate")) {
        service.SelectEntity(world->CloneEntity(entity));
    }
    Entity parent = world->GetParent(entity);
    if (parent != INVALID_ENTITY && ImGui::MenuItem(ICON_FA_ARROW_UP "  Detach from Parent")) {
        world->RemoveChild(parent, entity);
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, Palette::Error);
    if (ImGui::MenuItem(ICON_FA_TRASH_CAN "  Delete")) {
        const bool wasSelected = service.IsSelected(entity);
        world->DestroyEntity(entity);
        if (wasSelected) service.ClearSelection();
    }
    ImGui::PopStyleColor();

    ImGui::EndPopup();
}

void HierarchyEditor::DrawInsertionZone(IEditorService& service, Entity parent, Entity beforeSibling) {
    auto* world = service.GetWorld();

    // Two-level PushID gives each zone a unique scope without string allocation.
    ImGui::PushID((int)parent);
    ImGui::PushID(beforeSibling == INVALID_ENTITY ? -1 : (int)beforeSibling);

    const float zoneH = Theme::DropZoneHeight;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float zoneW = ImGui::GetContentRegionAvail().x;

    ImGui::InvisibleButton("##zone", ImVec2(zoneW > 0 ? zoneW : 1.0f, zoneH));

    if (ImGui::BeginDragDropTarget()) {
        // Visual insertion line while hovering.
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(origin.x, origin.y + zoneH * 0.5f),
            ImVec2(origin.x + zoneW, origin.y + zoneH * 0.5f),
            Palette::ToU32(Palette::Accent), Theme::DropLineWidth);

        const Entity dragged = AcceptEntityDrop(ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        // Dropping onto itself or into its own subtree would create a cycle.
        const bool valid = dragged != INVALID_ENTITY && dragged != parent &&
                           (parent == INVALID_ENTITY || !world->IsAncestorOf(dragged, parent));
        if (valid) {
            if (parent == INVALID_ENTITY) {
                // Root level: detach, then reorder unless dropped at the end.
                Detach(*world, dragged);
                if (beforeSibling != INVALID_ENTITY) world->MoveEntityBefore(dragged, beforeSibling);
            } else if (beforeSibling == INVALID_ENTITY) {
                AppendChild(*world, parent, dragged);
            } else {
                Detach(*world, dragged);
                world->InsertChildBefore(parent, dragged, beforeSibling);
                world->MoveEntityBefore(dragged, beforeSibling);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
    ImGui::PopID();
}

void HierarchyEditor::DrawHierarchyNode(IEditorService& service, Entity entity) {
    auto* world = service.GetWorld();

    // Copy children now — insertion zones can mutate childrenMap_ mid-frame.
    std::vector<Entity> children(world->GetChildren(entity));
    bool hasChildren = !children.empty();

    const std::string label = EntityLabel(world->GetEntityName(entity), entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (service.IsSelected(entity))
        flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID((int)entity);
    if (hasChildren) ApplyOpenRequest(openRequest_);
    bool open = ImGui::TreeNodeEx("##node", flags, "%s  %s", hasChildren ? ICON_FA_LAYER_GROUP : ICON_FA_CUBE,
                                  label.c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        service.SelectEntity(entity);

    // Drag source: let this node be dragged.
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(kEntityDragPayload, &entity, sizeof(Entity));
        ImGui::Text(ICON_FA_CUBE "  %s", label.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target on the node itself: make dragged entity a child of this one.
    if (ImGui::BeginDragDropTarget()) {
        const Entity dragged = AcceptEntityDrop();
        if (dragged != INVALID_ENTITY && dragged != entity && world->GetParent(dragged) != entity &&
            !world->IsAncestorOf(dragged, entity)) {
            AppendChild(*world, entity, dragged);
        }
        ImGui::EndDragDropTarget();
    }

    DrawEntityContextMenu(service, entity);

    // Children draw inside this node's ID scope; TreePop must match the TreeNodeEx push
    // before the PopID, or ImGui's tree stack desyncs.
    if (hasChildren && open) {
        for (Entity child : children) {
            DrawInsertionZone(service, entity, child);
            DrawHierarchyNode(service, child);
        }
        DrawInsertionZone(service, entity, INVALID_ENTITY);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void HierarchyEditor::DrawHierarchyTree(IEditorService& service) {
    auto* world = service.GetWorld();

    // Build a snapshot of root entities so insertion-zone drops don't invalidate iteration.
    std::vector<Entity> roots;
    for (Entity entity : world->GetLivingEntities()) {
        if (world->GetParent(entity) == INVALID_ENTITY && PassesFilters(*world, entity))
            roots.push_back(entity);
    }

    for (Entity root : roots) {
        // Insertion zone before each root: drop here to reorder at root level or unparent.
        DrawInsertionZone(service, INVALID_ENTITY, root);
        DrawHierarchyNode(service, root);
    }
    // Zone after the last root.
    DrawInsertionZone(service, INVALID_ENTITY, INVALID_ENTITY);
}

}  // namespace Elysium

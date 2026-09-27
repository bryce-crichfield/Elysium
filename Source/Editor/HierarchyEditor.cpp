#include "HierarchyEditor.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include "Core/Common.h"
#include "Core/Path.h"
#include "Core/World.h"
#include "Core/Entity.h"
#include "Core/PrefabInstance.h"
#include "Components/PrefabInstanceComponent.h"
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

constexpr const char* kSingleRootTip = "A prefab has a single root: create entities under it";
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
            // Delete / Ctrl+D act on the selection while the Hierarchy has focus.
            if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && !service.GetSelectedEntities().empty()) {
                const Entity primary = service.GetSelectedEntities().back();
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                    const std::vector<Entity> selection = service.GetSelectedEntities();
                    pendingAction_ = [&service, selection] { for (Entity e : selection) service.DeleteEntity(e); };
                } else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
                    pendingAction_ = [&service, primary] { service.DuplicateEntity(primary); };
                }
            }
            ImGui::EndChild();
            openRequest_.reset();

            if (pendingAction_) {
                pendingAction_();
                pendingAction_ = nullptr;
            }
            DrawCreatePrefabDialog(service);
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
    const bool canCreateRoot = service.CanBeRoot(INVALID_ENTITY);
    ImGui::BeginDisabled(!canCreateRoot);
    if (IconButton(ICON_FA_PLUS, "Create entity")) service.CreateEntity();
    ImGui::EndDisabled();
    if (!canCreateRoot) ItemTooltip(kSingleRootTip);
    ImGui::SameLine();
    if (IconButton(viewIcon, showHierarchyView_ ? "Show as flat list" : "Show as hierarchy")) {
        showHierarchyView_ = !showHierarchyView_;
    }
    ImGui::SameLine();
    if (luaFilterActive_) ImGui::PushStyleColor(ImGuiCol_Text, Palette().Accent);
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

void HierarchyEditor::DeferOpenPrefab(IEditorService& service, const World& world, Entity entity) {
    // Opening switches tabs, and so the world, which must wait until drawing is done.
    const std::string fullPath = world.GetComponent<PrefabInstanceComponent>(entity).FullPath();
    pendingAction_ = [&service, fullPath] { service.OpenPrefab(fullPath); };
}

void HierarchyEditor::BeginCreatePrefab(IEditorService& service, Entity entity) {
    namespace fs = std::filesystem;
    World* world = service.GetWorld();
    prefabDialog_.source = entity;
    prefabDialog_.open = true;

    // Default name: the entity's name as a file name ("Knight 1" -> "Knight_1").
    std::string name = world->GetEntityName(entity);
    name = name.substr(name.rfind("::") == std::string::npos ? 0 : name.rfind("::") + 2);
    for (char& ch : name) {
        if (!std::isalnum((unsigned char)ch) && ch != '_' && ch != '-') ch = '_';
    }
    snprintf(prefabDialog_.name, sizeof(prefabDialog_.name), "%s", name.empty() ? "NewPrefab" : name.c_str());

    // Every project folder is a candidate; default to Scenes/Prefabs.
    prefabDialog_.folders.clear();
    std::error_code ec;
    const fs::path root(Path::GetAssetsRoot());
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        const std::string leaf = it->path().filename().string();
        if (!leaf.empty() && leaf[0] == '.') {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_directory(ec)) prefabDialog_.folders.push_back(fs::relative(it->path(), root, ec).generic_string());
    }
    std::sort(prefabDialog_.folders.begin(), prefabDialog_.folders.end());
    const bool hasPrefabs = std::find(prefabDialog_.folders.begin(), prefabDialog_.folders.end(), "Scenes/Prefabs") != prefabDialog_.folders.end();
    prefabDialog_.folder = hasPrefabs ? "Scenes/Prefabs" : "";
}

void HierarchyEditor::DrawCreatePrefabDialog(IEditorService& service) {
    constexpr const char* kTitle = "Create Prefab";
    if (prefabDialog_.open) {
        ImGui::OpenPopup(kTitle);
        prefabDialog_.open = false;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(Theme().DialogWidth, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_NoSavedSettings)) return;

    World* world = service.GetWorld();
    if (!world || !world->IsAlive(prefabDialog_.source)) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    ImGui::TextDisabled("From %s and its children", EntityLabel(world->GetEntityName(prefabDialog_.source), prefabDialog_.source).c_str());
    ImGui::Spacing();

    PropertyLabel("Name");
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool submitted = ImGui::InputText("##name", prefabDialog_.name, sizeof(prefabDialog_.name), ImGuiInputTextFlags_EnterReturnsTrue);
    PropertyLabel("Folder");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##folder", prefabDialog_.folder.empty() ? "(project root)" : prefabDialog_.folder.c_str())) {
        for (size_t i = 0; i <= prefabDialog_.folders.size(); ++i) {
            const std::string folder = i == 0 ? "" : prefabDialog_.folders[i - 1];
            const bool selected = folder == prefabDialog_.folder;
            if (ImGui::Selectable(i == 0 ? "(project root)" : folder.c_str(), selected)) prefabDialog_.folder = folder;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Validate: a plain file name that doesn't clash with an existing file.
    const std::string name = prefabDialog_.name;
    const std::string relativePath = (prefabDialog_.folder.empty() ? "" : prefabDialog_.folder + "/") + name + ".xml";
    const std::string fullPath = Path::GetAssetsRoot() + relativePath;
    const char* problem = nullptr;
    if (name.empty()) problem = "Enter a name";
    else if (name.find_first_of("/\\:*?\"<>|") != std::string::npos) problem = "Name can't contain path characters";
    else if (std::filesystem::exists(fullPath)) problem = "A file with that name already exists";

    ImGui::Spacing();
    if (problem) ColoredText(Palette().Error, problem);
    else ImGui::TextDisabled("%s", relativePath.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    AlignRight(ButtonWidth("Create") + ButtonWidth("Cancel") + spacing);
    ImGui::BeginDisabled(problem != nullptr);
    const bool create = PrimaryButton("Create") || (submitted && !problem);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);

    if (create && !problem) {
        const Entity source = prefabDialog_.source;
        // Opening the prefab switches tabs, so do it after this frame's drawing.
        pendingAction_ = [&service, source, fullPath] { service.CreatePrefabFromEntity(source, fullPath); };
    }
    if (create || cancel) {
        prefabDialog_.source = INVALID_ENTITY;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, Theme().IdColumnWidth);

    for (Entity entity : world->GetLivingEntities()) {
        if (PrefabInstances::IsInternal(*world, entity) || !PassesFilters(*world, entity)) continue;

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
    if (ImGui::MenuItem(ICON_FA_PLUS "  Create Entity", nullptr, false, service.CanBeRoot(INVALID_ENTITY))) {
        service.CreateEntity();
    }
    if (!service.CanBeRoot(INVALID_ENTITY)) ItemTooltip(kSingleRootTip);
}

void HierarchyEditor::DrawEntityContextMenu(IEditorService& service, Entity entity) {
    auto* world = service.GetWorld();

    if (!ImGui::BeginPopupContextItem("EntityContextMenu")) return;

    ImGui::TextDisabled("%s", EntityLabel(world->GetEntityName(entity), entity).c_str());
    ImGui::Separator();

    // Deferred: the tree/list is mid-iteration over the world's entities.
    if (PrefabInstances::IsRoot(*world, entity)) {
        if (ImGui::MenuItem(ICON_FA_PEN_TO_SQUARE "  Open Prefab")) DeferOpenPrefab(service, *world, entity);
        ImGui::Separator();
    } else if (!world->GetChildren(entity).empty()) {
        if (ImGui::MenuItem(ICON_FA_BOX "  Create Prefab...")) BeginCreatePrefab(service, entity);
        ItemTooltip("Save this entity and its children as a new prefab file");
        ImGui::Separator();
    }
    if (ImGui::MenuItem(ICON_FA_PLUS "  Create Child")) {
        pendingAction_ = [&service, entity] { service.CreateEntity(entity); };
    }
    const bool isRoot = world->GetParent(entity) == INVALID_ENTITY;
    if (ImGui::MenuItem(ICON_FA_COPY "  Duplicate", "Ctrl+D", false, !isRoot || service.CanBeRoot(INVALID_ENTITY))) {
        pendingAction_ = [&service, entity] { service.DuplicateEntity(entity); };
    }
    Entity parent = world->GetParent(entity);
    if (parent != INVALID_ENTITY && service.CanBeRoot(entity) && ImGui::MenuItem(ICON_FA_ARROW_UP "  Detach from Parent")) {
        world->RemoveChild(parent, entity);
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, Palette().Error);
    if (ImGui::MenuItem(ICON_FA_TRASH_CAN "  Delete", "Del")) {
        pendingAction_ = [&service, entity] { service.DeleteEntity(entity); };
    }
    ImGui::PopStyleColor();

    ImGui::EndPopup();
}

void HierarchyEditor::DrawInsertionZone(IEditorService& service, Entity parent, Entity beforeSibling) {
    auto* world = service.GetWorld();

    // Two-level PushID gives each zone a unique scope without string allocation.
    ImGui::PushID((int)parent);
    ImGui::PushID(beforeSibling == INVALID_ENTITY ? -1 : (int)beforeSibling);

    const float zoneH = Theme().DropZoneHeight;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float zoneW = ImGui::GetContentRegionAvail().x;

    ImGui::InvisibleButton("##zone", ImVec2(zoneW > 0 ? zoneW : 1.0f, zoneH));

    if (ImGui::BeginDragDropTarget()) {
        // Visual insertion line while hovering.
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(origin.x, origin.y + zoneH * 0.5f),
            ImVec2(origin.x + zoneW, origin.y + zoneH * 0.5f),
            Palette().ToU32(Palette().Accent), Theme().DropLineWidth);

        const Entity dragged = AcceptEntityDrop(ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        // Dropping onto itself or into its own subtree would create a cycle.
        // In a prefab only the existing root may sit at root level.
        const bool valid = dragged != INVALID_ENTITY && dragged != parent &&
                           (parent == INVALID_ENTITY ? service.CanBeRoot(dragged) : !world->IsAncestorOf(dragged, parent));
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
    // A placed prefab is a black box: its own entities are hidden, only entities parented
    // to it from outside the instance show as children.
    std::vector<Entity> children;
    for (Entity child : world->GetChildren(entity)) {
        if (!PrefabInstances::IsInternal(*world, child)) children.push_back(child);
    }
    bool hasChildren = !children.empty();
    const bool isPrefab = PrefabInstances::IsRoot(*world, entity);

    const std::string label = EntityLabel(world->GetEntityName(entity), entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (service.IsSelected(entity))
        flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID((int)entity);
    if (hasChildren) ApplyOpenRequest(openRequest_);
    const char* icon = isPrefab ? ICON_FA_BOX : hasChildren ? ICON_FA_LAYER_GROUP : ICON_FA_CUBE;
    if (isPrefab) ImGui::PushStyleColor(ImGuiCol_Text, Palette().Accent);
    bool open = ImGui::TreeNodeEx("##node", flags, "%s  %s", icon, label.c_str());
    if (isPrefab) ImGui::PopStyleColor();
    if (isPrefab) ItemTooltip(("Prefab: " + world->GetComponent<PrefabInstanceComponent>(entity).src).c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        service.SelectEntity(entity);
    if (isPrefab && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        DeferOpenPrefab(service, *world, entity);

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

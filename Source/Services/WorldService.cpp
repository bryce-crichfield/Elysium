#include "Services/WorldService.h"
#include "Services/SceneService.h"
#include "Application.h"
#include "Entity.h"
#include "raylib.h"
#include "Common.h"
#include <algorithm>
#include <iostream>

namespace Elysium::Services
{

WorldService::WorldService()
{
    name_ = "WorldService";
}

void WorldService::Initialize()
{
    RegisterComponentTypes();
}

void WorldService::Shutdown()
{
    // Service cleanup if needed
}

void WorldService::RegisterComponentTypes()
{
    RegisterComponent<NameComponent>("Name");
    RegisterComponent<PositionComponent>("Position");
    RegisterComponent<LocationComponent>("Location");
    RegisterComponent<MovementComponent>("Movement");
    RegisterComponent<AnimationComponent>("Animation");
    RegisterComponent<DirectionComponent>("Direction");
    RegisterComponent<LayerComponent>("Layer");
    RegisterComponent<RectangleComponent>("Rectangle");
    RegisterComponent<CircleComponent>("Circle");
    RegisterComponent<LightComponent>("Light");
    RegisterComponent<SpriteComponent>("Sprite");
    RegisterComponent<TextComponent>("Text");
    RegisterComponent<CameraComponent>("Camera");
    RegisterComponent<FollowComponent>("Follow");
    RegisterComponent<TileComponent>("Tile");
    RegisterComponent<TeamComponent>("Team");
    RegisterComponent<CooldownComponent>("Cooldown");
    RegisterComponent<CharacterComponent>("Character");
    RegisterComponent<UnitComponent>("Unit");
}

void WorldService::Update(float deltaTime)
{
    Profile;
    auto& app = Elysium::Application::GetInstance();
    auto& sceneService = app.GetService<SceneService>();
    auto newWorld = sceneService.GetScene() ? sceneService.GetScene()->GetWorld() : nullptr;

    if (newWorld != world) {
        world = newWorld;
    }
}

void WorldService::ImGui()
{
    Profile;
    if (!world) {
        ImGui::Text("No World Loaded");
        return;
    }

    // Left side header
    ImGui::Text("Entities");
    ImGui::SameLine(leftPanelWidth + 10); // Position right side header
    ImGui::Text("Inspector");

    // Left panel - Entity List
    ImGui::BeginChild("EntityPanel", ImVec2(leftPanelWidth, 0), true);
    DrawEntityToolbar();

    // Scrollable entity table
    ImGui::BeginChild("EntityList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    DrawEntityList();
    ImGui::EndChild();
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
        if (!isDraggingSplitter)
        {
            // First frame of dragging - don't apply delta yet, just mark as dragging
            isDraggingSplitter = true;
        }
        else
        {
            // Subsequent frames - apply mouse delta
            leftPanelWidth += ImGui::GetIO().MouseDelta.x;
            if (leftPanelWidth < 200.0f)
                leftPanelWidth = 200.0f;
            if (leftPanelWidth > ImGui::GetWindowWidth() - 200.0f)
                leftPanelWidth = ImGui::GetWindowWidth() - 200.0f;
        }
    }
    else
    {
        isDraggingSplitter = false;
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    // Right panel - Inspector
    ImGui::BeginChild("Inspector", ImVec2(0, 0), true);
    DrawInspectorToolbar();
    DrawInspectorPanel();
    ImGui::EndChild();
}

void WorldService::DrawEntityToolbar()
{
    ImGui::Text("Search: ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    static char searchBuffer[256] = "";
    if (ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer)))
    {
        searchFilter = std::string(searchBuffer);
    }
    ImGui::Separator();

    // Filter panel
    if (ImGui::CollapsingHeader("Filters", filtersCollapsed ? 0 : ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Clear button at the top
        if (ImGui::Button("Clear All Filters"))
        {
            filters.clear();
        }

        // Filter table
        if (!filters.empty() && ImGui::BeginTable("Filters", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("NOT", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Op", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < filters.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                // Negation checkbox
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("##negate", &filters[i].negate);

                // Component dropdown
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1);
                std::vector<const char *> componentNames;
                int currentSelection = -1;
                for (size_t j = 0; j < componentPlaceholders.size(); ++j)
                {
                    componentNames.push_back(componentPlaceholders[j].name.c_str());
                    if (componentPlaceholders[j].name == filters[i].componentName)
                    {
                        currentSelection = static_cast<int>(j);
                    }
                }

                if (ImGui::Combo("##component", &currentSelection, componentNames.data(), componentNames.size()))
                {
                    if (currentSelection >= 0)
                    {
                        filters[i].componentName = componentPlaceholders[currentSelection].name;
                        filters[i].predicate = componentPlaceholders[currentSelection].hasComponentFunc;
                    }
                }

                // Operator dropdown (skip for first filter)
                ImGui::TableSetColumnIndex(2);
                if (i > 0)
                {
                    const char *operators[] = {"AND", "OR"};
                    int currentOp = static_cast<int>(filters[i].logicalOperator);
                    if (ImGui::Combo("##operator", &currentOp, operators, IM_ARRAYSIZE(operators)))
                    {
                        filters[i].logicalOperator = static_cast<FilterLogicalOperator>(currentOp);
                    }
                }
                else
                {
                    ImGui::Text("-");
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        // Add/Remove buttons at the bottom
        if (ImGui::Button("Add Filter"))
        {
            filters.emplace_back();
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Filter") && !filters.empty())
        {
            filters.pop_back();
        }
    }

    ImGui::Separator();

    // Entity management buttons
    bool canDeselect = selectedEntity != INVALID_ENTITY;
    if (!canDeselect)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }
    if (ImGui::Button("Deselect"))
    {
        if (canDeselect)
            DeselectEntity();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove"))
    {
        if (canDeselect)
            RemoveEntity();
    }
    if (!canDeselect)
    {
        ImGui::PopStyleVar();
    }
    ImGui::SameLine();
    if (ImGui::Button("Create"))
    {
        CreateEntity();
    }
}

void WorldService::DrawEntityList()
{
    if (ImGui::BeginTable("Entities", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const auto &livingEntities = world->GetEntitiesWithComponents<>();

        for (Entity entity : livingEntities)
        {
            bool shouldDraw = true;
            if (!filters.empty())
            {
                // Evaluate first filter (only if it has a valid predicate)
                if (filters[0].predicate)
                {
                    shouldDraw = filters[0].Evaluate(entity, world);
                }

                // Evaluate remaining filters in order
                for (size_t i = 1; i < filters.size(); ++i)
                {
                    if (filters[i].predicate)
                    {
                        bool filterResult = filters[i].Evaluate(entity, world);

                        if (filters[i].logicalOperator == FilterLogicalOperator::AND)
                        {
                            shouldDraw = shouldDraw && filterResult;
                        }
                        else // OR
                        {
                            shouldDraw = shouldDraw || filterResult;
                        }
                    }
                }
            }

            if (!shouldDraw)
                continue;

            // Get the actual entity name if it exists, otherwise use generic name
            std::string entityName = world->GetEntityName(entity);
            if (entityName.empty())
            {
                entityName = "Entity " + std::to_string(entity);
            }

            if (!searchFilter.empty())
            {
                std::string lowerName = entityName;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::string lowerFilter = searchFilter;
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

                if (lowerName.find(lowerFilter) == std::string::npos)
                {
                    continue;
                }
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            bool isSelected = (entity == selectedEntity);
            if (ImGui::Selectable(std::to_string(entity).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
            {
                selectedEntity = entity;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", entityName.c_str());
        }

        ImGui::EndTable();
    }
}

void WorldService::DrawInspectorToolbar()
{
    if (selectedEntity != INVALID_ENTITY)
    {
        // Get the actual entity name if it exists, otherwise use generic name
        std::string entityName = world->GetEntityName(selectedEntity);
        if (entityName.empty())
        {
            entityName = "Entity " + std::to_string(selectedEntity);
        }

        ImGui::Text("Entity ID: %zu", selectedEntity);
        ImGui::SameLine();
        ImGui::Text("Name: %s", entityName.c_str());

        ImGui::Separator();

        if (ImGui::Button("Add Component"))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            for (const auto &placeholder : componentPlaceholders)
            {
                if (!placeholder.hasComponentFunc(selectedEntity, world))
                {
                    if (ImGui::MenuItem(placeholder.name.c_str()))
                    {
                        placeholder.addComponentFunc(selectedEntity, world);
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndPopup();
        }
    }
    else
    {
        ImGui::Text("No entity selected");
    }
}

void WorldService::DrawInspectorPanel()
{
    if (selectedEntity == INVALID_ENTITY)
        return;

    // Clear any previous deletion request
    componentToDelete = "";

    for (const auto &placeholder : componentPlaceholders)
    {
        if (placeholder.hasComponentFunc(selectedEntity, world))
        {
            DrawComponentPanel(placeholder);
        }
    }

    // Process deletion after the loop to avoid iterator invalidation
    if (!componentToDelete.empty())
    {
        for (const auto &placeholder : componentPlaceholders)
        {
            if (placeholder.name == componentToDelete)
            {
                placeholder.removeComponentFunc(selectedEntity, world);
                break;
            }
        }
        componentToDelete = "";
    }
}

void WorldService::DrawComponentPanel(const ComponentPlaceholder &placeholder)
{
    ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

    bool nodeOpen = ImGui::TreeNodeEx(placeholder.name.c_str(), treeFlags);

    // ImGui::SameLine();
    ImGui::PushID(placeholder.name.c_str());
    ImGui::PopID();

    if (nodeOpen)
    {
        if (ImGui::SmallButton("Delete"))
        {
            componentToDelete = placeholder.name;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Reset"))
        {
            placeholder.resetComponentFunc(selectedEntity, world);
        }

        ImGui::Separator();

        placeholder.drawFunc(selectedEntity, world);
        ImGui::TreePop();
    }
}

void WorldService::DeselectEntity()
{
    selectedEntity = INVALID_ENTITY;
}

void WorldService::RemoveEntity()
{
    if (selectedEntity != INVALID_ENTITY)
    {
        world->DestroyEntity(selectedEntity);
        selectedEntity = INVALID_ENTITY;
    }
}

void WorldService::CreateEntity()
{
    Entity newEntity = world->CreateEntity();
    selectedEntity = newEntity;
}

#define FIELD_LABEL(text)                                                                                              \
    ImGui::AlignTextToFramePadding();                                                                                  \
    ImGui::Text(text);                                                                                                 \
    ImGui::SameLine(140.0f);                                                                                           \
    ImGui::SetNextItemWidth(-1);

template <> void WorldService::DrawComponent<NameComponent>(Entity entity, Elysium::World *world)
{
    auto &nameComp = world->GetComponent<NameComponent>(entity);

    static char nameBuffer[256];
    strncpy(nameBuffer, nameComp.name.c_str(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';

    FIELD_LABEL("Name: ")
    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
    {
        nameComp.name = std::string(nameBuffer);
    }
}

template <> void WorldService::DrawComponent<PositionComponent>(Entity entity, Elysium::World *world)
{
    auto &pos = world->GetComponent<PositionComponent>(entity);
    FIELD_LABEL("X: ")
    ImGui::DragFloat("##X", &pos.x, 1.0f);
    FIELD_LABEL("Y: ")
    ImGui::DragFloat("##Y", &pos.y, 1.0f);
}

template <> void WorldService::DrawComponent<LocationComponent>(Entity entity, Elysium::World *world)
{
    auto &loc = world->GetComponent<LocationComponent>(entity);
    FIELD_LABEL("X: ")
    ImGui::DragInt("##X", &loc.x);
    FIELD_LABEL("Y: ")
    ImGui::DragInt("##Y", &loc.y);
}

template <> void WorldService::DrawComponent<MovementComponent>(Entity entity, Elysium::World *world)
{
    auto &movement = world->GetComponent<MovementComponent>(entity);
    FIELD_LABEL("Speed: ")
    ImGui::DragFloat("##Speed", &movement.speed, 1.0f, 0.0f, 1000.0f);
    FIELD_LABEL("Is Moving: ")
    ImGui::Checkbox("##IsMoving", &movement.isMoving);
    FIELD_LABEL("Loop: ")
    ImGui::Checkbox("##Loop", &movement.loop);
    ImGui::Text("Waypoints: %zu", movement.waypoints.size());
    ImGui::Text("Current Waypoint: %zu", movement.currentWaypointIndex);
}

template <> void WorldService::DrawComponent<AnimationComponent>(Entity entity, Elysium::World *world)
{
    auto &anim = world->GetComponent<AnimationComponent>(entity);

    static char markerBuffer[256];
    strncpy(markerBuffer, anim.marker.c_str(), sizeof(markerBuffer) - 1);
    markerBuffer[sizeof(markerBuffer) - 1] = '\0';

    FIELD_LABEL("Marker: ")
    if (ImGui::InputText("##Marker", markerBuffer, sizeof(markerBuffer)))
    {
        anim.marker = std::string(markerBuffer);
    }

    FIELD_LABEL("Current Frame: ")
    ImGui::DragInt("##CurrentFrame", &anim.currentFrame, 1.0f, 0);
    FIELD_LABEL("Start Frame: ")
    ImGui::DragInt("##StartFrame", &anim.start, 1.0f, 0);
    FIELD_LABEL("End Frame: ")
    ImGui::DragInt("##EndFrame", &anim.end, 1.0f, 0);
    FIELD_LABEL("Duration: ")
    ImGui::DragFloat("##FrameDuration", &anim.frameDuration, 0.01f, 0.0f, 10.0f);
    FIELD_LABEL("Elapsed: ")
    ImGui::DragFloat("##Elapsed", &anim.elapsed, 0.01f, 0.0f);
    FIELD_LABEL("Loop: ")
    ImGui::Checkbox("##Loop", &anim.loop);
}

template <> void WorldService::DrawComponent<DirectionComponent>(Entity entity, Elysium::World *world)
{
    auto &dir = world->GetComponent<DirectionComponent>(entity);

    const char *directions[] = {"NONE", "UP", "DOWN", "LEFT", "RIGHT"};
    int currentDir = static_cast<int>(dir.currentDirection);
    int previousDir = static_cast<int>(dir.previousDirection);

    FIELD_LABEL("Current Dir: ")
    if (ImGui::Combo("##CurrentDirection", &currentDir, directions, IM_ARRAYSIZE(directions)))
    {
        dir.SetDirection(static_cast<Direction>(currentDir));
    }

    FIELD_LABEL("Previous Dir: ")
    ImGui::Combo("##PreviousDirection", &previousDir, directions, IM_ARRAYSIZE(directions));
    FIELD_LABEL("Has Changed: ")
    ImGui::Checkbox("##HasChanged", &dir.hasChanged);
}

template <> void WorldService::DrawComponent<LayerComponent>(Entity entity, Elysium::World *world)
{
    auto &layer = world->GetComponent<LayerComponent>(entity);

    FIELD_LABEL("Z Index: ")
    ImGui::DragInt("##ZIndex", &layer.zIndex);

    const char *types[] = {"Background", "World", "Lighting", "Overlay"};
    int typeIndex = static_cast<int>(layer.type);
    FIELD_LABEL("Type: ")
    if (ImGui::Combo("##Type", &typeIndex, types, IM_ARRAYSIZE(types)))
    {
        layer.type = static_cast<LayerComponent::Type>(typeIndex);
    }

    const char *spaces[] = {"World", "Screen", "Parallax"};
    int spaceIndex = static_cast<int>(layer.space);
    FIELD_LABEL("Space: ")
    if (ImGui::Combo("##Space", &spaceIndex, spaces, IM_ARRAYSIZE(spaces)))
    {
        layer.space = static_cast<LayerComponent::Space>(spaceIndex);
    }

    const char *blends[] = {"Normal", "Additive", "Multiply", "Alpha"};
    int blendIndex = static_cast<int>(layer.blend);
    FIELD_LABEL("Blend: ")
    if (ImGui::Combo("##Blend", &blendIndex, blends, IM_ARRAYSIZE(blends)))
    {
        layer.blend = static_cast<LayerComponent::Blend>(blendIndex);
    }

    FIELD_LABEL("Opacity: ")
    ImGui::DragFloat("##Opacity", &layer.opacity, 0.01f, 0.0f, 1.0f);
    FIELD_LABEL("Is Visible: ")
    ImGui::Checkbox("##IsVisible", &layer.isVisible);

    static char nameBuffer[256];
    strncpy(nameBuffer, layer.name.c_str(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';

    FIELD_LABEL("Name: ")
    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
    {
        layer.name = std::string(nameBuffer);
    }

    FIELD_LABEL("Parallax: ")
    ImGui::DragFloat2("##ParallaxFactor", &layer.parallaxFactor.x, 0.01f, 0.0f, 1.0f);
}

template <> void WorldService::DrawComponent<RectangleComponent>(Entity entity, Elysium::World *world)
{
    auto &rect = world->GetComponent<RectangleComponent>(entity);

    FIELD_LABEL("Width: ")
    ImGui::DragFloat("##Width", &rect.width, 1.0f, 1.0f, 1000.0f);
    FIELD_LABEL("Height: ")
    ImGui::DragFloat("##Height", &rect.height, 1.0f, 1.0f, 1000.0f);

    float bg[4] = {rect.background.r / 255.0f, rect.background.g / 255.0f, rect.background.b / 255.0f,
                   rect.background.a / 255.0f};
    FIELD_LABEL("Background: ")
    if (ImGui::ColorEdit4("##Background", bg))
    {
        rect.background = {(unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255),
                           (unsigned char)(bg[3] * 255)};
    }

    float border[4] = {rect.border.r / 255.0f, rect.border.g / 255.0f, rect.border.b / 255.0f, rect.border.a / 255.0f};
    FIELD_LABEL("Border: ")
    if (ImGui::ColorEdit4("##Border", border))
    {
        rect.border = {(unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255),
                       (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255)};
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, rect.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        rect.layerName = std::string(layerBuffer);
    }
}

template <> void WorldService::DrawComponent<CircleComponent>(Entity entity, Elysium::World *world)
{
    auto &circle = world->GetComponent<CircleComponent>(entity);

    FIELD_LABEL("Radius: ")
    ImGui::DragFloat("##Radius", &circle.radius, 1.0f, 1.0f, 1000.0f);

    float bg[4] = {circle.background.r / 255.0f, circle.background.g / 255.0f, circle.background.b / 255.0f,
                   circle.background.a / 255.0f};
    FIELD_LABEL("Background: ")
    if (ImGui::ColorEdit4("##Background", bg))
    {
        circle.background = {(unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255),
                             (unsigned char)(bg[3] * 255)};
    }

    float border[4] = {circle.border.r / 255.0f, circle.border.g / 255.0f, circle.border.b / 255.0f,
                       circle.border.a / 255.0f};
    FIELD_LABEL("Border: ")
    if (ImGui::ColorEdit4("##Border", border))
    {
        circle.border = {(unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255),
                         (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255)};
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, circle.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        circle.layerName = std::string(layerBuffer);
    }
}

template <> void WorldService::DrawComponent<LightComponent>(Entity entity, Elysium::World *world)
{
    auto &light = world->GetComponent<LightComponent>(entity);

    float color[4] = {light.color.r / 255.0f, light.color.g / 255.0f, light.color.b / 255.0f, light.color.a / 255.0f};
    FIELD_LABEL("Color: ")
    if (ImGui::ColorEdit4("##Color", color))
    {
        light.color = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255),
                       (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255)};
    }

    FIELD_LABEL("Radius: ")
    ImGui::DragFloat("##Radius", &light.radius, 1.0f, 1.0f, 1000.0f);

    static char layerBuffer[256];
    strncpy(layerBuffer, light.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        light.layerName = std::string(layerBuffer);
    }
}

template <> void WorldService::DrawComponent<SpriteComponent>(Entity entity, Elysium::World *world)
{
    auto &sprite = world->GetComponent<SpriteComponent>(entity);

    static char markerBuffer[256];
    strncpy(markerBuffer, sprite.markerName.c_str(), sizeof(markerBuffer) - 1);
    markerBuffer[sizeof(markerBuffer) - 1] = '\0';

    FIELD_LABEL("Marker Name: ")
    if (ImGui::InputText("##MarkerName", markerBuffer, sizeof(markerBuffer)))
    {
        sprite.markerName = std::string(markerBuffer);
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, sprite.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        sprite.layerName = std::string(layerBuffer);
    }

    FIELD_LABEL("Frame Index: ")
    ImGui::DragInt("##FrameIndex", &sprite.frameIndex, 1.0f, 0);
    FIELD_LABEL("Duration: ")
    ImGui::DragFloat("##FrameDuration", &sprite.frameDuration, 0.01f, 0.0f, 10.0f);
    FIELD_LABEL("Elapsed: ")
    ImGui::DragFloat("##FrameElapsed", &sprite.frameElapsed, 0.01f, 0.0f);
}

template <> void WorldService::DrawComponent<TextComponent>(Entity entity, Elysium::World *world)
{
    auto &text = world->GetComponent<TextComponent>(entity);

    static char contentBuffer[1024];
    strncpy(contentBuffer, text.content.c_str(), sizeof(contentBuffer) - 1);
    contentBuffer[sizeof(contentBuffer) - 1] = '\0';

    FIELD_LABEL("Content: ")
    if (ImGui::InputTextMultiline("##Content", contentBuffer, sizeof(contentBuffer)))
    {
        text.content = std::string(contentBuffer);
    }

    FIELD_LABEL("Font Size: ")
    ImGui::DragInt("##FontSize", &text.fontSize, 1.0f, 1, 200);

    float color[4] = {text.color.r / 255.0f, text.color.g / 255.0f, text.color.b / 255.0f, text.color.a / 255.0f};
    FIELD_LABEL("Color: ")
    if (ImGui::ColorEdit4("##Color", color))
    {
        text.color = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255), (unsigned char)(color[2] * 255),
                      (unsigned char)(color[3] * 255)};
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, text.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        text.layerName = std::string(layerBuffer);
    }
}

template <> void WorldService::DrawComponent<CameraComponent>(Entity entity, Elysium::World *world)
{
    auto &camera = world->GetComponent<CameraComponent>(entity);

    FIELD_LABEL("Zoom: ")
    ImGui::DragFloat("##Zoom", &camera.zoom, 0.01f, 0.1f, 10.0f);
    FIELD_LABEL("Viewport: ")
    ImGui::DragFloat4("##Viewport", &camera.viewport.x, 1.0f);
    FIELD_LABEL("Render Order: ")
    ImGui::DragInt("##RenderOrder", &camera.renderOrder);
    FIELD_LABEL("Is Visible: ")
    ImGui::Checkbox("##IsVisible", &camera.isVisible);
}

template <> void WorldService::DrawComponent<FollowComponent>(Entity entity, Elysium::World *world)
{
    auto &follow = world->GetComponent<FollowComponent>(entity);

    static char targetBuffer[256];
    strncpy(targetBuffer, follow.targetEntityName.c_str(), sizeof(targetBuffer) - 1);
    targetBuffer[sizeof(targetBuffer) - 1] = '\0';

    FIELD_LABEL("Follow Speed: ")
    ImGui::DragFloat("##FollowSpeed", &follow.speed, 0.1f, 0.0f);

    FIELD_LABEL("Target: ")
    if (ImGui::InputText("##TargetEntityName", targetBuffer, sizeof(targetBuffer)))
    {
        follow.targetEntityName = std::string(targetBuffer);
    }

}

template <> void WorldService::DrawComponent<TileComponent>(Entity entity, Elysium::World *world)
{
    ImGui::Text("Tile component (no properties)");
}

template <> void WorldService::DrawComponent<TeamComponent>(Entity entity, Elysium::World *world)
{
    auto &team = world->GetComponent<TeamComponent>(entity);
    FIELD_LABEL("Team ID: ")
    ImGui::DragInt("##TeamID", &team.teamId, 1.0f, 0);
}

template <> void WorldService::DrawComponent<CooldownComponent>(Entity entity, Elysium::World *world)
{
    auto &cooldown = world->GetComponent<CooldownComponent>(entity);

    FIELD_LABEL("Cooldown Time: ")
    ImGui::DragFloat("##CooldownTime", &cooldown.cooldownTime, 0.1f, 0.0f, 60.0f);
    FIELD_LABEL("Elapsed Time: ")
    ImGui::DragFloat("##ElapsedTime", &cooldown.elapsedTime, 0.01f, 0.0f);
    FIELD_LABEL("On Cooldown: ")
    ImGui::Checkbox("##IsOnCooldown", &cooldown.isOnCooldown);
}

template <> void WorldService::DrawComponent<CharacterComponent>(Entity entity, Elysium::World *world)
{
    auto &character = world->GetComponent<CharacterComponent>(entity);
    FIELD_LABEL("Char ID: ")
    ImGui::DragInt("##CharacterID", &character.id, 1.0f, 0);
}

template <> void WorldService::DrawComponent<UnitComponent>(Entity entity, Elysium::World *world)
{
    auto &unit = world->GetComponent<UnitComponent>(entity);

    FIELD_LABEL("Acted: ")
    ImGui::Checkbox("##HasActedThisTurn", &unit.hasActedThisTurn);
    FIELD_LABEL("Can Move: ")
    ImGui::Checkbox("##CanMove", &unit.canMove);
    FIELD_LABEL("Can Attack: ")
    ImGui::Checkbox("##CanAttack", &unit.canAttack);
    FIELD_LABEL("Can Cast Spells: ")
    ImGui::Checkbox("##CanCastSpells", &unit.canCastSpells);
    FIELD_LABEL("Can Use Items: ")
    ImGui::Checkbox("##CanUseItems", &unit.canUseItems);

    if (ImGui::Button("Start Turn"))
    {
        unit.StartTurn();
    }
    ImGui::SameLine();
    if (ImGui::Button("End Turn"))
    {
        unit.EndTurn();
    }
}

} // namespace Elysium::Services

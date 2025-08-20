#include "Services/InspectorService.h"
#include "raylib.h"
#include <iostream>
#include <algorithm>
#include "Entity.h"

namespace Elysium {

void InspectorService::Initialize()
{
    RegisterComponentTypes();
}

void InspectorService::Shutdown()
{
    // Service cleanup if needed
}

void InspectorService::RegisterComponentTypes()
{
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

    for (auto& placeholder : componentPlaceholders)
    {
        filterStates[placeholder.name] = true; // Default to showing all components
    }
}


void InspectorService::Update(float deltaTime)
{
    if (!showInspector) return;
}


void InspectorService::Draw()
{
    if (!showInspector || !world) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float inspectorWidth = viewport->WorkSize.x * 0.6f;
    float inspectorHeight = viewport->WorkSize.y * 0.8f;

    ImGui::SetNextWindowSize(ImVec2(inspectorWidth, inspectorHeight), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 50, viewport->WorkPos.y + 50), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Entity Inspector", &showInspector))
    {
        static float leftPanelWidth = inspectorWidth * 0.4f;

        // Left panel - Entity List
        ImGui::BeginChild("EntityList", ImVec2(leftPanelWidth, 0), true);
            DrawEntityToolbar();
            DrawEntityList();
        ImGui::EndChild();

        ImGui::SameLine();

        // Splitter
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::Button("##splitter", ImVec2(4.0f, -1));
        if (ImGui::IsItemActive())
        {
            leftPanelWidth += ImGui::GetIO().MouseDelta.x;
            if (leftPanelWidth < 200.0f) leftPanelWidth = 200.0f;
            if (leftPanelWidth > inspectorWidth - 200.0f) leftPanelWidth = inspectorWidth - 200.0f;
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
    ImGui::End();
}

void InspectorService::DrawEntityToolbar()
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

    bool canDeselect = selectedEntity != INVALID_ENTITY;
    if (!canDeselect)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }
    if (ImGui::Button("Deselect"))
    {
        if (canDeselect) DeselectEntity();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove"))
    {
        if (canDeselect) RemoveEntity();
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
    ImGui::Separator();

    // Filter panel
    ImGui::Text("Component Filter:");
    ImGui::Separator();

    // Filter mode dropdown
    const char* filterModes[] = { "Some Of", "All Of", "None Of" };
    static int currentFilterMode = 1; // Default to "All Of"
    if (ImGui::Combo("Filter Mode", &currentFilterMode, filterModes, IM_ARRAYSIZE(filterModes)))
    {
        // Update currentFilter based on currentFilterMode
        switch (currentFilterMode)
        {
        case 0: currentFilter = InspectorFilter::SomeOf; break;
        case 1: currentFilter = InspectorFilter::AllOf; break;
        case 2: currentFilter = InspectorFilter::NoneOf; break;
        }
    }
    ImGui::Separator();

    // Grid layout for checkboxes
    const int columns = 3; // Adjust based on UI width or preference
    ImGui::BeginTable("ComponentFilterTable", columns, ImGuiTableFlags_SizingStretchProp);
    for (auto& placeholder : componentPlaceholders)
    {
        ImGui::TableNextColumn();
        ImGui::Checkbox(placeholder.name.c_str(), &filterStates[placeholder.name]);
    }
    ImGui::EndTable();
    ImGui::Separator();
}

void InspectorService::DrawEntityList()
{
    if (ImGui::BeginTable("Entities", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const auto& livingEntities = world->GetEntitiesWithComponents<>();

        for (Entity entity : livingEntities)
        {
            bool shouldDraw = true;
            switch (currentFilter)
            {
            case InspectorFilter::SomeOf:
                shouldDraw = false;
                for (const auto& placeholder : componentPlaceholders)
                {
                    // Only check components that are enabled in filterStates
                    if (filterStates[placeholder.name] && placeholder.hasComponentFunc(entity, world))
                    {
                        shouldDraw = true;
                        break;
                    }
                }
                break;
            case InspectorFilter::AllOf:
                shouldDraw = true;
                for (const auto& placeholder : componentPlaceholders)
                {
                    // Only check components that are enabled in filterStates
                    if (filterStates[placeholder.name] && !placeholder.hasComponentFunc(entity, world))
                    {
                        shouldDraw = false;
                        break;
                    }
                }
                break;
            case InspectorFilter::NoneOf:
                shouldDraw = true;
                for (const auto& placeholder : componentPlaceholders)
                {
                    // Only check components that are enabled in filterStates
                    if (filterStates[placeholder.name] && placeholder.hasComponentFunc(entity, world))
                    {
                        shouldDraw = false;
                        break;
                    }
                }
                break;
            default:
                break;
            }

            if (!shouldDraw) continue;

            std::string entityName = "Entity " + std::to_string(entity);

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

void InspectorService::DrawInspectorToolbar()
{
    if (selectedEntity != INVALID_ENTITY)
    {
        ImGui::Text("Entity ID: %zu", selectedEntity);
        ImGui::SameLine();
        ImGui::Text("Name: Entity %zu", selectedEntity);

        ImGui::Separator();

        if (ImGui::Button("Add Component"))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            for (const auto& placeholder : componentPlaceholders)
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

void InspectorService::DrawInspectorPanel()
{
    if (selectedEntity == INVALID_ENTITY) return;

    // Clear any previous deletion request
    componentToDelete = "";

    for (const auto& placeholder : componentPlaceholders)
    {
        if (placeholder.hasComponentFunc(selectedEntity, world))
        {
            DrawComponentPanel(placeholder);
        }
    }

    // Process deletion after the loop to avoid iterator invalidation
    if (!componentToDelete.empty())
    {
        for (const auto& placeholder : componentPlaceholders)
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

void InspectorService::DrawComponentPanel(const ComponentPlaceholder& placeholder)
{
    ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

    bool nodeOpen = ImGui::TreeNodeEx(placeholder.name.c_str(), treeFlags);

    ImGui::SameLine();
    ImGui::PushID(placeholder.name.c_str());

    if (ImGui::SmallButton("Delete"))
    {
        componentToDelete = placeholder.name;
        ImGui::PopID();
        if (nodeOpen) ImGui::TreePop();
        return;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Reset"))
    {
        placeholder.resetComponentFunc(selectedEntity, world);
    }

    ImGui::PopID();

    if (nodeOpen)
    {
        placeholder.drawFunc(selectedEntity, world);
        ImGui::TreePop();
    }
}

void InspectorService::DeselectEntity()
{
    selectedEntity = INVALID_ENTITY;
}

void InspectorService::RemoveEntity()
{
    if (selectedEntity != INVALID_ENTITY)
    {
        world->DestroyEntity(selectedEntity);
        selectedEntity = INVALID_ENTITY;
    }
}

void InspectorService::CreateEntity()
{
    Entity newEntity = world->CreateEntity();
    selectedEntity = newEntity;
}

template<>
void InspectorService::DrawComponent<PositionComponent>(Entity entity, World* world)
{
    auto& pos = world->GetComponent<PositionComponent>(entity);
    ImGui::Text("X:"); ImGui::SameLine(); ImGui::DragFloat("##X", &pos.x, 1.0f);
    ImGui::Text("Y:"); ImGui::SameLine(); ImGui::DragFloat("##Y", &pos.y, 1.0f);
}

template<>
void InspectorService::DrawComponent<LocationComponent>(Entity entity, World* world)
{
    auto& loc = world->GetComponent<LocationComponent>(entity);
    ImGui::Text("X:"); ImGui::SameLine(); ImGui::DragInt("##X", &loc.x);
    ImGui::Text("Y:"); ImGui::SameLine(); ImGui::DragInt("##Y", &loc.y);
}

template<>
void InspectorService::DrawComponent<MovementComponent>(Entity entity, World* world)
{
    auto& movement = world->GetComponent<MovementComponent>(entity);
    ImGui::Text("Speed:"); ImGui::SameLine(); ImGui::DragFloat("##Speed", &movement.speed, 1.0f, 0.0f, 1000.0f);
    ImGui::Text("Is Moving:"); ImGui::SameLine(); ImGui::Checkbox("##IsMoving", &movement.isMoving);
    ImGui::Text("Loop:"); ImGui::SameLine(); ImGui::Checkbox("##Loop", &movement.loop);
    ImGui::Text("Waypoints: %zu", movement.waypoints.size());
    ImGui::Text("Current Waypoint: %zu", movement.currentWaypointIndex);
}

template<>
void InspectorService::DrawComponent<AnimationComponent>(Entity entity, World* world)
{
    auto& anim = world->GetComponent<AnimationComponent>(entity);

    static char markerBuffer[256];
    strncpy(markerBuffer, anim.marker.c_str(), sizeof(markerBuffer) - 1);
    markerBuffer[sizeof(markerBuffer) - 1] = '\0';

    ImGui::Text("Marker:"); ImGui::SameLine();
    if (ImGui::InputText("##Marker", markerBuffer, sizeof(markerBuffer)))
    {
        anim.marker = std::string(markerBuffer);
    }

    ImGui::Text("Current Frame:"); ImGui::SameLine(); ImGui::DragInt("##CurrentFrame", &anim.currentFrame, 1.0f, 0);
    ImGui::Text("Start Frame:"); ImGui::SameLine(); ImGui::DragInt("##StartFrame", &anim.start, 1.0f, 0);
    ImGui::Text("End Frame:"); ImGui::SameLine(); ImGui::DragInt("##EndFrame", &anim.end, 1.0f, 0);
    ImGui::Text("Frame Duration:"); ImGui::SameLine(); ImGui::DragFloat("##FrameDuration", &anim.frameDuration, 0.01f, 0.0f, 10.0f);
    ImGui::Text("Elapsed:"); ImGui::SameLine(); ImGui::DragFloat("##Elapsed", &anim.elapsed, 0.01f, 0.0f);
    ImGui::Text("Loop:"); ImGui::SameLine(); ImGui::Checkbox("##Loop", &anim.loop);
}

template<>
void InspectorService::DrawComponent<DirectionComponent>(Entity entity, World* world)
{
    auto& dir = world->GetComponent<DirectionComponent>(entity);

    const char* directions[] = { "NONE", "UP", "DOWN", "LEFT", "RIGHT" };
    int currentDir = static_cast<int>(dir.currentDirection);
    int previousDir = static_cast<int>(dir.previousDirection);

    ImGui::Text("Current Direction:"); ImGui::SameLine();
    if (ImGui::Combo("##CurrentDirection", &currentDir, directions, IM_ARRAYSIZE(directions)))
    {
        dir.SetDirection(static_cast<Direction>(currentDir));
    }

    ImGui::Text("Previous Direction:"); ImGui::SameLine(); ImGui::Combo("##PreviousDirection", &previousDir, directions, IM_ARRAYSIZE(directions));
    ImGui::Text("Has Changed:"); ImGui::SameLine(); ImGui::Checkbox("##HasChanged", &dir.hasChanged);
}

template<>
void InspectorService::DrawComponent<LayerComponent>(Entity entity, World* world)
{
    auto& layer = world->GetComponent<LayerComponent>(entity);

    ImGui::Text("Z Index:"); ImGui::SameLine(); ImGui::DragInt("##ZIndex", &layer.zIndex);

    const char* types[] = { "Background", "World", "Lighting", "Overlay" };
    int typeIndex = static_cast<int>(layer.type);
    ImGui::Text("Type:"); ImGui::SameLine();
    if (ImGui::Combo("##Type", &typeIndex, types, IM_ARRAYSIZE(types)))
    {
        layer.type = static_cast<LayerComponent::Type>(typeIndex);
    }

    const char* spaces[] = { "World", "Screen", "Parallax" };
    int spaceIndex = static_cast<int>(layer.space);
    ImGui::Text("Space:"); ImGui::SameLine();
    if (ImGui::Combo("##Space", &spaceIndex, spaces, IM_ARRAYSIZE(spaces)))
    {
        layer.space = static_cast<LayerComponent::Space>(spaceIndex);
    }

    const char* blends[] = { "Normal", "Additive", "Multiply", "Alpha" };
    int blendIndex = static_cast<int>(layer.blend);
    ImGui::Text("Blend:"); ImGui::SameLine();
    if (ImGui::Combo("##Blend", &blendIndex, blends, IM_ARRAYSIZE(blends)))
    {
        layer.blend = static_cast<LayerComponent::Blend>(blendIndex);
    }

    ImGui::Text("Opacity:"); ImGui::SameLine(); ImGui::DragFloat("##Opacity", &layer.opacity, 0.01f, 0.0f, 1.0f);
    ImGui::Text("Is Visible:"); ImGui::SameLine(); ImGui::Checkbox("##IsVisible", &layer.isVisible);

    static char nameBuffer[256];
    strncpy(nameBuffer, layer.name.c_str(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';

    ImGui::Text("Name:"); ImGui::SameLine();
    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
    {
        layer.name = std::string(nameBuffer);
    }

    ImGui::Text("Parallax Factor:"); ImGui::SameLine(); ImGui::DragFloat2("##ParallaxFactor", &layer.parallaxFactor.x, 0.01f, 0.0f, 1.0f);
}

template<>
void InspectorService::DrawComponent<RectangleComponent>(Entity entity, World* world)
{
    auto& rect = world->GetComponent<RectangleComponent>(entity);

    ImGui::Text("Width:"); ImGui::SameLine(); ImGui::DragFloat("##Width", &rect.width, 1.0f, 1.0f, 1000.0f);
    ImGui::Text("Height:"); ImGui::SameLine(); ImGui::DragFloat("##Height", &rect.height, 1.0f, 1.0f, 1000.0f);

    float bg[4] = { rect.background.r / 255.0f, rect.background.g / 255.0f, rect.background.b / 255.0f, rect.background.a / 255.0f };
    ImGui::Text("Background:"); ImGui::SameLine();
    if (ImGui::ColorEdit4("##Background", bg))
    {
        rect.background = { (unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255), (unsigned char)(bg[3] * 255) };
    }

    float border[4] = { rect.border.r / 255.0f, rect.border.g / 255.0f, rect.border.b / 255.0f, rect.border.a / 255.0f };
    ImGui::Text("Border:"); ImGui::SameLine();
    if (ImGui::ColorEdit4("##Border", border))
    {
        rect.border = { (unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255), (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255) };
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, rect.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    ImGui::Text("Layer Name:"); ImGui::SameLine();
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        rect.layerName = std::string(layerBuffer);
    }
}

template<>
void InspectorService::DrawComponent<CircleComponent>(Entity entity, World* world)
{
    auto& circle = world->GetComponent<CircleComponent>(entity);

    ImGui::Text("Radius:"); ImGui::SameLine(); ImGui::DragFloat("##Radius", &circle.radius, 1.0f, 1.0f, 1000.0f);

    float bg[4] = { circle.background.r / 255.0f, circle.background.g / 255.0f, circle.background.b / 255.0f, circle.background.a / 255.0f };
    ImGui::Text("Background:"); ImGui::SameLine();
    if (ImGui::ColorEdit4("##Background", bg))
    {
        circle.background = { (unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255), (unsigned char)(bg[3] * 255) };
    }

    float border[4] = { circle.border.r / 255.0f, circle.border.g / 255.0f, circle.border.b / 255.0f, circle.border.a / 255.0f };
    ImGui::Text("Border:"); ImGui::SameLine();
    if (ImGui::ColorEdit4("##Border", border))
    {
        circle.border = { (unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255), (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255) };
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, circle.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    ImGui::Text("Layer Name:"); ImGui::SameLine();
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        circle.layerName = std::string(layerBuffer);
    }
}

template<>
void InspectorService::DrawComponent<LightComponent>(Entity entity, World* world)
{
    auto& light = world->GetComponent<LightComponent>(entity);

    float color[4] = { light.color.r / 255.0f, light.color.g / 255.0f, light.color.b / 255.0f, light.color.a / 255.0f };
    ImGui::Text("Color:"); ImGui::SameLine();
    if (ImGui::ColorEdit4("##Color", color))
    {
        light.color = { (unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255), (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255) };
    }

    ImGui::Text("Radius:"); ImGui::SameLine(); ImGui::DragFloat("##Radius", &light.radius, 1.0f, 1.0f, 1000.0f);

    static char layerBuffer[256];
    strncpy(layerBuffer, light.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    ImGui::Text("Layer Name:"); ImGui::SameLine();
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        light.layerName = std::string(layerBuffer);
    }
}

template<>
void InspectorService::DrawComponent<SpriteComponent>(Entity entity, World* world)
{
    auto& sprite = world->GetComponent<SpriteComponent>(entity);

    static char markerBuffer[256];
    strncpy(markerBuffer, sprite.markerName.c_str(), sizeof(markerBuffer) - 1);
    markerBuffer[sizeof(markerBuffer) - 1] = '\0';

    ImGui::Text("Marker Name:"); ImGui::SameLine();
    if (ImGui::InputText("##MarkerName", markerBuffer, sizeof(markerBuffer)))
    {
        sprite.markerName = std::string(markerBuffer);
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, sprite.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    ImGui::Text("Layer Name:"); ImGui::SameLine();
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        sprite.layerName = std::string(layerBuffer);
    }

    ImGui::Text("Frame Index:"); ImGui::SameLine(); ImGui::DragInt("##FrameIndex", &sprite.frameIndex, 1.0f, 0);
    ImGui::Text("Frame Duration:"); ImGui::SameLine(); ImGui::DragFloat("##FrameDuration", &sprite.frameDuration, 0.01f, 0.0f, 10.0f);
    ImGui::Text("Frame Elapsed:"); ImGui::SameLine(); ImGui::DragFloat("##FrameElapsed", &sprite.frameElapsed, 0.01f, 0.0f);
}

template<>
void InspectorService::DrawComponent<TextComponent>(Entity entity, World* world)
{
    auto& text = world->GetComponent<TextComponent>(entity);

    static char contentBuffer[1024];
    strncpy(contentBuffer, text.content.c_str(), sizeof(contentBuffer) - 1);
    contentBuffer[sizeof(contentBuffer) - 1] = '\0';

    ImGui::Text("Content:"); ImGui::SameLine();
    if (ImGui::InputTextMultiline("##Content", contentBuffer, sizeof(contentBuffer)))
    {
        text.content = std::string(contentBuffer);
    }

    ImGui::Text("Font Size:"); ImGui::SameLine(); ImGui::DragInt("##FontSize", &text.fontSize, 1.0f, 1, 200);

    float color[4] = { text.color.r / 255.0f, text.color.g / 255.0f, text.color.b / 255.0f, text.color.a / 255.0f };
    ImGui::Text("Color:"); ImGui::SameLine();
    if (ImGui::ColorEdit4("##Color", color))
    {
        text.color = { (unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255), (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255) };
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, text.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    ImGui::Text("Layer Name:"); ImGui::SameLine();
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer)))
    {
        text.layerName = std::string(layerBuffer);
    }
}

template<>
void InspectorService::DrawComponent<CameraComponent>(Entity entity, World* world)
{
    auto& camera = world->GetComponent<CameraComponent>(entity);

    ImGui::Text("Position:"); ImGui::SameLine(); ImGui::DragFloat2("##Position", &camera.position.x, 1.0f);
    ImGui::Text("Zoom:"); ImGui::SameLine(); ImGui::DragFloat("##Zoom", &camera.zoom, 0.01f, 0.1f, 10.0f);
    ImGui::Text("Viewport:"); ImGui::SameLine(); ImGui::DragFloat4("##Viewport", &camera.viewport.x, 1.0f);
    ImGui::Text("Render Order:"); ImGui::SameLine(); ImGui::DragInt("##RenderOrder", &camera.renderOrder);
    ImGui::Text("Is Visible:"); ImGui::SameLine(); ImGui::Checkbox("##IsVisible", &camera.isVisible);
}

template<>
void InspectorService::DrawComponent<FollowComponent>(Entity entity, World* world)
{
    auto& follow = world->GetComponent<FollowComponent>(entity);

    static char targetBuffer[256];
    strncpy(targetBuffer, follow.targetEntityName.c_str(), sizeof(targetBuffer) - 1);
    targetBuffer[sizeof(targetBuffer) - 1] = '\0';

    ImGui::Text("Target Entity Name:"); ImGui::SameLine();
    if (ImGui::InputText("##TargetEntityName", targetBuffer, sizeof(targetBuffer)))
    {
        follow.targetEntityName = std::string(targetBuffer);
    }
}

template<>
void InspectorService::DrawComponent<TileComponent>(Entity entity, World* world)
{
    ImGui::Text("Tile component (no properties)");
}

template<>
void InspectorService::DrawComponent<TeamComponent>(Entity entity, World* world)
{
    auto& team = world->GetComponent<TeamComponent>(entity);
    ImGui::Text("Team ID:"); ImGui::SameLine(); ImGui::DragInt("##TeamID", &team.teamId, 1.0f, 0);
}

template<>
void InspectorService::DrawComponent<CooldownComponent>(Entity entity, World* world)
{
    auto& cooldown = world->GetComponent<CooldownComponent>(entity);

    ImGui::Text("Cooldown Time:"); ImGui::SameLine(); ImGui::DragFloat("##CooldownTime", &cooldown.cooldownTime, 0.1f, 0.0f, 60.0f);
    ImGui::Text("Elapsed Time:"); ImGui::SameLine(); ImGui::DragFloat("##ElapsedTime", &cooldown.elapsedTime, 0.01f, 0.0f);
    ImGui::Text("Is On Cooldown:"); ImGui::SameLine(); ImGui::Checkbox("##IsOnCooldown", &cooldown.isOnCooldown);
}

template<>
void InspectorService::DrawComponent<CharacterComponent>(Entity entity, World* world)
{
    auto& character = world->GetComponent<CharacterComponent>(entity);
    ImGui::Text("Character ID:"); ImGui::SameLine(); ImGui::DragInt("##CharacterID", &character.id, 1.0f, 0);
}

template<>
void InspectorService::DrawComponent<UnitComponent>(Entity entity, World* world)
{
    auto& unit = world->GetComponent<UnitComponent>(entity);

    ImGui::Text("Has Acted This Turn:"); ImGui::SameLine(); ImGui::Checkbox("##HasActedThisTurn", &unit.hasActedThisTurn);
    ImGui::Text("Can Move:"); ImGui::SameLine(); ImGui::Checkbox("##CanMove", &unit.canMove);
    ImGui::Text("Can Attack:"); ImGui::SameLine(); ImGui::Checkbox("##CanAttack", &unit.canAttack);
    ImGui::Text("Can Cast Spells:"); ImGui::SameLine(); ImGui::Checkbox("##CanCastSpells", &unit.canCastSpells);
    ImGui::Text("Can Use Items:"); ImGui::SameLine(); ImGui::Checkbox("##CanUseItems", &unit.canUseItems);

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

} // namespace Elysium

#include "Services/WorldService.h"
#include <algorithm>
#include <iostream>
#include <set>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Services/AssetService.h"
#include "Services/SceneService.h"
#include "Services/ScriptService.h"
#include "imgui.h"
#include "raylib.h"

namespace Elysium::Services {

WorldService::WorldService() {
    name_ = "WorldService";
}

void WorldService::Initialize() {
    RegisterComponentTypes();
}

void WorldService::Shutdown() {
    // Service cleanup if needed
}

void WorldService::RegisterComponentTypes() {
    RegisterComponent<NameComponent>("Name");
    RegisterComponent<PositionComponent>("Position");
    RegisterComponent<ScaleComponent>("Scale");
    RegisterComponent<LocationComponent>("Location");
    RegisterComponent<MovementComponent>("Movement");
    RegisterComponent<AnimationComponent>("Animation");
    RegisterComponent<DirectionComponent>("Direction");
    RegisterComponent<LayerComponent>("Layer");
    RegisterComponent<RectangleComponent>("Rectangle");
    RegisterComponent<CircleComponent>("Circle");
    RegisterComponent<LightComponent>("Light");
    RegisterComponent<SpriteComponent>("Sprite");
    RegisterComponent<TextureComponent>("Texture");
    RegisterComponent<TextComponent>("Text");
    RegisterComponent<CameraComponent>("Camera");
    RegisterComponent<FollowComponent>("Follow");
    RegisterComponent<TileComponent>("Tile");
    RegisterComponent<TeamComponent>("Team");
    RegisterComponent<CooldownComponent>("Cooldown");
    RegisterComponent<CharacterComponent>("Character");
    RegisterComponent<UnitComponent>("Unit");
    RegisterComponent<BoundsComponent>("Bounds");
    RegisterComponent<ScriptComponent>("Script");
}

void WorldService::Update(float deltaTime) {
    Profile;
    auto& app = Elysium::Application::GetInstance();
    auto& sceneService = app.GetService<SceneService>();
    auto newWorld = sceneService.GetScene() ? sceneService.GetScene()->GetWorld() : nullptr;

    if (newWorld != world) {
        world = newWorld;
    }

    // Auto-select dragged entities
    if (world) {
        world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
            if (bounds.isDragging) {
                selectedEntity = entity;
            }
        });
    }
}

#define FIELD_LABEL(text)             \
    ImGui::AlignTextToFramePadding(); \
    ImGui::Text(text);                \
    ImGui::SameLine(140.0f);          \
    ImGui::SetNextItemWidth(-1);

template <>
void WorldService::DrawComponent<NameComponent>(Entity entity, Elysium::World* world) {
    auto& nameComp = world->GetComponent<NameComponent>(entity);

    static char nameBuffer[256];
    strncpy(nameBuffer, nameComp.name.c_str(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';

    FIELD_LABEL("Name: ")
    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
        nameComp.name = std::string(nameBuffer);
    }
}

template <>
void WorldService::DrawComponent<PositionComponent>(Entity entity, Elysium::World* world) {
    auto& pos = world->GetComponent<PositionComponent>(entity);
    FIELD_LABEL("X: ")
    std::string xId = "##PosX_" + std::to_string(entity);
    ImGui::DragFloat(xId.c_str(), &pos.x, 1.0f);
    FIELD_LABEL("Y: ")
    std::string yId = "##PosY_" + std::to_string(entity);
    ImGui::DragFloat(yId.c_str(), &pos.y, 1.0f);
}

template <>
void WorldService::DrawComponent<ScaleComponent>(Entity entity, Elysium::World* world) {
    auto& scale = world->GetComponent<ScaleComponent>(entity);
    FIELD_LABEL("X: ")
    std::string xId = "##ScaleX_" + std::to_string(entity);
    ImGui::DragFloat(xId.c_str(), &scale.x, 0.01f, 0.0f);
    FIELD_LABEL("Y: ")
    std::string yId = "##ScaleY_" + std::to_string(entity);
    ImGui::DragFloat(yId.c_str(), &scale.y, 0.01f, 0.0f);
}

template <>
void WorldService::DrawComponent<LocationComponent>(Entity entity, Elysium::World* world) {
    auto& loc = world->GetComponent<LocationComponent>(entity);
    FIELD_LABEL("X: ")
    ImGui::DragInt("##X", &loc.x);
    FIELD_LABEL("Y: ")
    ImGui::DragInt("##Y", &loc.y);
}

template <>
void WorldService::DrawComponent<MovementComponent>(Entity entity, Elysium::World* world) {
    auto& movement = world->GetComponent<MovementComponent>(entity);
    FIELD_LABEL("Speed: ")
    ImGui::DragFloat("##Speed", &movement.speed, 1.0f, 0.0f, 1000.0f);
    FIELD_LABEL("Is Moving: ")
    ImGui::Checkbox("##IsMoving", &movement.isMoving);
    FIELD_LABEL("Loop: ")
    ImGui::Checkbox("##Loop", &movement.loop);
    ImGui::Text("Waypoints: %zu", movement.waypoints.size());
    ImGui::Text("Current Waypoint: %zu", movement.currentWaypointIndex);
}

template <>
void WorldService::DrawComponent<AnimationComponent>(Entity entity, Elysium::World* world) {
    auto& anim = world->GetComponent<AnimationComponent>(entity);

    static char markerBuffer[256];
    strncpy(markerBuffer, anim.marker.c_str(), sizeof(markerBuffer) - 1);
    markerBuffer[sizeof(markerBuffer) - 1] = '\0';

    FIELD_LABEL("Marker: ")
    if (ImGui::InputText("##Marker", markerBuffer, sizeof(markerBuffer))) {
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

template <>
void WorldService::DrawComponent<DirectionComponent>(Entity entity, Elysium::World* world) {
    auto& dir = world->GetComponent<DirectionComponent>(entity);

    const char* directions[] = {"NONE", "UP", "DOWN", "LEFT", "RIGHT"};
    int currentDir = static_cast<int>(dir.currentDirection);
    int previousDir = static_cast<int>(dir.previousDirection);

    FIELD_LABEL("Current Dir: ")
    if (ImGui::Combo("##CurrentDirection", &currentDir, directions, IM_ARRAYSIZE(directions))) {
        dir.SetDirection(static_cast<Direction>(currentDir));
    }

    FIELD_LABEL("Previous Dir: ")
    ImGui::Combo("##PreviousDirection", &previousDir, directions, IM_ARRAYSIZE(directions));
    FIELD_LABEL("Has Changed: ")
    ImGui::Checkbox("##HasChanged", &dir.hasChanged);
}

template <>
void WorldService::DrawComponent<LayerComponent>(Entity entity, Elysium::World* world) {
    auto& layer = world->GetComponent<LayerComponent>(entity);

    FIELD_LABEL("Z Index: ")
    ImGui::DragInt("##ZIndex", &layer.zIndex);

    const char* types[] = {"Background", "World", "Lighting", "Overlay"};
    int typeIndex = static_cast<int>(layer.type);
    FIELD_LABEL("Type: ")
    if (ImGui::Combo("##Type", &typeIndex, types, IM_ARRAYSIZE(types))) {
        layer.type = static_cast<LayerComponent::Type>(typeIndex);
    }

    const char* spaces[] = {"World", "Screen", "Parallax"};
    int spaceIndex = static_cast<int>(layer.space);
    FIELD_LABEL("Space: ")
    if (ImGui::Combo("##Space", &spaceIndex, spaces, IM_ARRAYSIZE(spaces))) {
        layer.space = static_cast<LayerComponent::Space>(spaceIndex);
    }

    const char* blends[] = {"Normal", "Additive", "Multiply", "Alpha"};
    int blendIndex = static_cast<int>(layer.blend);
    FIELD_LABEL("Blend: ")
    if (ImGui::Combo("##Blend", &blendIndex, blends, IM_ARRAYSIZE(blends))) {
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
    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
        layer.name = std::string(nameBuffer);
    }

    FIELD_LABEL("Parallax: ")
    ImGui::DragFloat2("##ParallaxFactor", &layer.parallaxFactor.x, 0.01f, 0.0f, 1.0f);
}

template <>
void WorldService::DrawComponent<RectangleComponent>(Entity entity, Elysium::World* world) {
    auto& rect = world->GetComponent<RectangleComponent>(entity);

    FIELD_LABEL("Width: ")
    ImGui::DragFloat("##Width", &rect.width, 1.0f, 1.0f, 1000.0f);
    FIELD_LABEL("Height: ")
    ImGui::DragFloat("##Height", &rect.height, 1.0f, 1.0f, 1000.0f);

    float bg[4] = {rect.background.r / 255.0f, rect.background.g / 255.0f, rect.background.b / 255.0f,
                   rect.background.a / 255.0f};
    FIELD_LABEL("Background: ")
    if (ImGui::ColorEdit4("##Background", bg)) {
        rect.background = {(unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255),
                           (unsigned char)(bg[3] * 255)};
    }

    float border[4] = {rect.border.r / 255.0f, rect.border.g / 255.0f, rect.border.b / 255.0f, rect.border.a / 255.0f};
    FIELD_LABEL("Border: ")
    if (ImGui::ColorEdit4("##Border", border)) {
        rect.border = {(unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255),
                       (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255)};
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, rect.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer))) {
        rect.layerName = std::string(layerBuffer);
    }
}

template <>
void WorldService::DrawComponent<CircleComponent>(Entity entity, Elysium::World* world) {
    auto& circle = world->GetComponent<CircleComponent>(entity);

    FIELD_LABEL("Radius: ")
    ImGui::DragFloat("##Radius", &circle.radius, 1.0f, 1.0f, 1000.0f);

    float bg[4] = {circle.background.r / 255.0f, circle.background.g / 255.0f, circle.background.b / 255.0f,
                   circle.background.a / 255.0f};
    FIELD_LABEL("Background: ")
    if (ImGui::ColorEdit4("##Background", bg)) {
        circle.background = {(unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255),
                             (unsigned char)(bg[3] * 255)};
    }

    float border[4] = {circle.border.r / 255.0f, circle.border.g / 255.0f, circle.border.b / 255.0f,
                       circle.border.a / 255.0f};
    FIELD_LABEL("Border: ")
    if (ImGui::ColorEdit4("##Border", border)) {
        circle.border = {(unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255),
                         (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255)};
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, circle.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer))) {
        circle.layerName = std::string(layerBuffer);
    }
}

template <>
void WorldService::DrawComponent<LightComponent>(Entity entity, Elysium::World* world) {
    auto& light = world->GetComponent<LightComponent>(entity);

    float color[4] = {light.color.r / 255.0f, light.color.g / 255.0f, light.color.b / 255.0f, light.color.a / 255.0f};
    FIELD_LABEL("Color: ")
    if (ImGui::ColorEdit4("##Color", color)) {
        light.color = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255),
                       (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255)};
    }

    FIELD_LABEL("Radius: ")
    ImGui::DragFloat("##Radius", &light.radius, 1.0f, 1.0f, 1000.0f);

    static char layerBuffer[256];
    strncpy(layerBuffer, light.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer))) {
        light.layerName = std::string(layerBuffer);
    }
}

template <>
void WorldService::DrawComponent<SpriteComponent>(Entity entity, Elysium::World* world) {
    auto& sprite = world->GetComponent<SpriteComponent>(entity);

    // Sprite asset picker
    FIELD_LABEL("Sprite Asset: ")
    auto& assetService = Elysium::Application::GetInstance().GetService<AssetService>();
    const auto& allAssets = assetService.GetAllAssets();

    // Build list of sprite assets (non-static to avoid conflicts)
    std::vector<std::string> spriteAssetNames;
    spriteAssetNames.push_back("<None>");

    for (const auto& [name, asset] : allAssets) {
        if (asset.GetType() == AssetType::SPRITE && asset.IsLoaded()) {
            spriteAssetNames.push_back(name);
        }
    }

    // Find current selection
    std::string currentSpriteName = sprite.sprite.name.empty() ? "<None>" : sprite.sprite.name;
    int currentIndex = 0;
    for (size_t i = 0; i < spriteAssetNames.size(); ++i) {
        if (spriteAssetNames[i] == currentSpriteName) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    std::string assetComboId = "##SpriteAsset_" + std::to_string(entity);
    if (ImGui::BeginCombo(assetComboId.c_str(), currentSpriteName.c_str())) {
        for (size_t i = 0; i < spriteAssetNames.size(); ++i) {
            bool isSelected = (currentIndex == static_cast<int>(i));
            std::string selectableId = spriteAssetNames[i] + "##" + std::to_string(i);
            if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                if (i == 0) {
                    // Clear sprite
                    sprite.sprite = Sprite();
                } else {
                    // Assign sprite from asset service
                    sprite.sprite = assetService.GetSprite(spriteAssetNames[i]);
                }
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Marker picker (only if sprite is loaded)
    FIELD_LABEL("Marker: ")
    if (!sprite.sprite.name.empty() && !sprite.sprite.sheets.empty()) {
        // Collect unique markers from all sheets (deduplicated)
        std::vector<std::string> markerNames;
        markerNames.push_back("<None>");

        std::set<std::string> uniqueMarkers;
        for (const auto& [sheetName, sheet] : sprite.sprite.sheets) {
            for (const auto& [markerName, marker] : sheet.markers) {
                uniqueMarkers.insert(markerName);
            }
        }

        for (const auto& markerName : uniqueMarkers) {
            markerNames.push_back(markerName);
        }

        // Find current selection
        std::string currentMarker = sprite.markerName.empty() ? "<None>" : sprite.markerName;
        int markerIndex = 0;
        for (size_t i = 0; i < markerNames.size(); ++i) {
            if (markerNames[i] == currentMarker) {
                markerIndex = static_cast<int>(i);
                break;
            }
        }

        std::string comboId = "##SpriteMarker_" + std::to_string(entity);
        if (ImGui::BeginCombo(comboId.c_str(), currentMarker.c_str())) {
            for (size_t i = 0; i < markerNames.size(); ++i) {
                bool isSelected = (markerIndex == static_cast<int>(i));
                std::string selectableId = markerNames[i] + "##" + std::to_string(i);
                if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                    sprite.markerName = (i == 0) ? "" : markerNames[i];
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    } else {
        // Fallback to text input if no sprite loaded
        static char markerBuffer[256];
        strncpy(markerBuffer, sprite.markerName.c_str(), sizeof(markerBuffer) - 1);
        markerBuffer[sizeof(markerBuffer) - 1] = '\0';

        std::string inputId = "##SpriteMarkerInput_" + std::to_string(entity);
        if (ImGui::InputText(inputId.c_str(), markerBuffer, sizeof(markerBuffer))) {
            sprite.markerName = std::string(markerBuffer);
        }
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, sprite.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    std::string layerInputId = "##SpriteLayerName_" + std::to_string(entity);
    if (ImGui::InputText(layerInputId.c_str(), layerBuffer, sizeof(layerBuffer))) {
        sprite.layerName = std::string(layerBuffer);
    }

    FIELD_LABEL("Frame Index: ")
    std::string frameIndexId = "##SpriteFrameIndex_" + std::to_string(entity);
    ImGui::DragInt(frameIndexId.c_str(), &sprite.frameIndex, 1.0f, 0);

    FIELD_LABEL("Duration: ")
    std::string durationId = "##SpriteFrameDuration_" + std::to_string(entity);
    ImGui::DragFloat(durationId.c_str(), &sprite.frameDuration, 0.01f, 0.0f, 10.0f);

    FIELD_LABEL("Elapsed: ")
    std::string elapsedId = "##SpriteFrameElapsed_" + std::to_string(entity);
    ImGui::DragFloat(elapsedId.c_str(), &sprite.frameElapsed, 0.01f, 0.0f);
}

template <>
void WorldService::DrawComponent<TextureComponent>(Entity entity, Elysium::World* world) {
    auto& texture = world->GetComponent<TextureComponent>(entity);

    // Texture asset picker
    FIELD_LABEL("Texture Asset: ")
    auto& assetService = Elysium::Application::GetInstance().GetService<AssetService>();
    const auto& allAssets = assetService.GetAllAssets();

    // Build list of texture assets
    std::vector<std::string> textureAssetNames;
    textureAssetNames.push_back("<None>");

    for (const auto& [name, asset] : allAssets) {
        if (asset.GetType() == AssetType::TEXTURE && asset.IsLoaded()) {
            textureAssetNames.push_back(name);
        }
    }

    // Find current selection
    std::string currentTextureName = texture.textureName.empty() ? "<None>" : texture.textureName;
    int currentIndex = 0;
    for (size_t i = 0; i < textureAssetNames.size(); ++i) {
        if (textureAssetNames[i] == currentTextureName) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    std::string assetComboId = "##TextureAsset_" + std::to_string(entity);
    if (ImGui::BeginCombo(assetComboId.c_str(), currentTextureName.c_str())) {
        for (size_t i = 0; i < textureAssetNames.size(); ++i) {
            bool isSelected = (currentIndex == static_cast<int>(i));
            std::string selectableId = textureAssetNames[i] + "##" + std::to_string(i);
            if (ImGui::Selectable(selectableId.c_str(), isSelected)) {
                texture.textureName = (i == 0) ? "" : textureAssetNames[i];
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, texture.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    std::string layerInputId = "##TextureLayerName_" + std::to_string(entity);
    if (ImGui::InputText(layerInputId.c_str(), layerBuffer, sizeof(layerBuffer))) {
        texture.layerName = std::string(layerBuffer);
    }

    FIELD_LABEL("Clip Rect: ")
    ImGui::Text("X:");
    ImGui::SameLine();
    std::string clipXId = "##TextureClipX_" + std::to_string(entity);
    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat(clipXId.c_str(), &texture.clip.x, 1.0f, 0.0f);
    ImGui::SameLine();
    ImGui::Text("Y:");
    ImGui::SameLine();
    std::string clipYId = "##TextureClipY_" + std::to_string(entity);
    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat(clipYId.c_str(), &texture.clip.y, 1.0f, 0.0f);
    ImGui::Text("W:");
    ImGui::SameLine();
    std::string clipWId = "##TextureClipW_" + std::to_string(entity);
    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat(clipWId.c_str(), &texture.clip.width, 1.0f, 0.0f);
    ImGui::SameLine();
    ImGui::Text("H:");
    ImGui::SameLine();
    std::string clipHId = "##TextureClipH_" + std::to_string(entity);
    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat(clipHId.c_str(), &texture.clip.height, 1.0f, 0.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("(0 = full texture)");

    FIELD_LABEL("Tint: ")
    std::string tintId = "##TextureTint_" + std::to_string(entity);
    float tintColor[4] = {
        texture.tint.r / 255.0f,
        texture.tint.g / 255.0f,
        texture.tint.b / 255.0f,
        texture.tint.a / 255.0f};
    if (ImGui::ColorEdit4(tintId.c_str(), tintColor)) {
        texture.tint = {
            static_cast<unsigned char>(tintColor[0] * 255),
            static_cast<unsigned char>(tintColor[1] * 255),
            static_cast<unsigned char>(tintColor[2] * 255),
            static_cast<unsigned char>(tintColor[3] * 255)};
    }
}

template <>
void WorldService::DrawComponent<TextComponent>(Entity entity, Elysium::World* world) {
    auto& text = world->GetComponent<TextComponent>(entity);

    static char contentBuffer[1024];
    strncpy(contentBuffer, text.content.c_str(), sizeof(contentBuffer) - 1);
    contentBuffer[sizeof(contentBuffer) - 1] = '\0';

    FIELD_LABEL("Content: ")
    if (ImGui::InputTextMultiline("##Content", contentBuffer, sizeof(contentBuffer))) {
        text.content = std::string(contentBuffer);
    }

    FIELD_LABEL("Font Size: ")
    ImGui::DragInt("##FontSize", &text.fontSize, 1.0f, 1, 200);

    float color[4] = {text.color.r / 255.0f, text.color.g / 255.0f, text.color.b / 255.0f, text.color.a / 255.0f};
    FIELD_LABEL("Color: ")
    if (ImGui::ColorEdit4("##Color", color)) {
        text.color = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255), (unsigned char)(color[2] * 255),
                      (unsigned char)(color[3] * 255)};
    }

    static char layerBuffer[256];
    strncpy(layerBuffer, text.layerName.c_str(), sizeof(layerBuffer) - 1);
    layerBuffer[sizeof(layerBuffer) - 1] = '\0';

    FIELD_LABEL("Layer Name: ")
    if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer))) {
        text.layerName = std::string(layerBuffer);
    }
}

template <>
void WorldService::DrawComponent<CameraComponent>(Entity entity, Elysium::World* world) {
    auto& camera = world->GetComponent<CameraComponent>(entity);

    FIELD_LABEL("Zoom: ")
    ImGui::DragFloat("##Zoom", &camera.zoom, 0.01f, 0.1f, 10.0f);
    FIELD_LABEL("Viewport: ")
    ImGui::DragFloat4("##Viewport", &camera.viewport.x, 1.0f);
    FIELD_LABEL("Render Order: ")
    ImGui::DragInt("##RenderOrder", &camera.renderOrder);
    FIELD_LABEL("Is Visible: ")
    ImGui::Checkbox("##IsVisible", &camera.isVisible);
}

template <>
void WorldService::DrawComponent<FollowComponent>(Entity entity, Elysium::World* world) {
    auto& follow = world->GetComponent<FollowComponent>(entity);

    static char targetBuffer[256];
    strncpy(targetBuffer, follow.targetEntityName.c_str(), sizeof(targetBuffer) - 1);
    targetBuffer[sizeof(targetBuffer) - 1] = '\0';

    FIELD_LABEL("Follow Speed: ")
    ImGui::DragFloat("##FollowSpeed", &follow.speed, 0.1f, 0.0f);

    FIELD_LABEL("Target: ")
    if (ImGui::InputText("##TargetEntityName", targetBuffer, sizeof(targetBuffer))) {
        follow.targetEntityName = std::string(targetBuffer);
    }
}

template <>
void WorldService::DrawComponent<TileComponent>(Entity entity, Elysium::World* world) {
    ImGui::Text("Tile component (no properties)");
}

template <>
void WorldService::DrawComponent<TeamComponent>(Entity entity, Elysium::World* world) {
    auto& team = world->GetComponent<TeamComponent>(entity);
    FIELD_LABEL("Team ID: ")
    ImGui::DragInt("##TeamID", &team.teamId, 1.0f, 0);
}

template <>
void WorldService::DrawComponent<CooldownComponent>(Entity entity, Elysium::World* world) {
    auto& cooldown = world->GetComponent<CooldownComponent>(entity);

    FIELD_LABEL("Cooldown Time: ")
    ImGui::DragFloat("##CooldownTime", &cooldown.cooldownTime, 0.1f, 0.0f, 60.0f);
    FIELD_LABEL("Elapsed Time: ")
    ImGui::DragFloat("##ElapsedTime", &cooldown.elapsedTime, 0.01f, 0.0f);
    FIELD_LABEL("On Cooldown: ")
    ImGui::Checkbox("##IsOnCooldown", &cooldown.isOnCooldown);
}

template <>
void WorldService::DrawComponent<CharacterComponent>(Entity entity, Elysium::World* world) {
    auto& character = world->GetComponent<CharacterComponent>(entity);
    FIELD_LABEL("Char ID: ")
    ImGui::DragInt("##CharacterID", &character.id, 1.0f, 0);
}

template <>
void WorldService::DrawComponent<UnitComponent>(Entity entity, Elysium::World* world) {
    auto& unit = world->GetComponent<UnitComponent>(entity);

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

    if (ImGui::Button("Start Turn")) {
        unit.StartTurn();
    }
    ImGui::SameLine();
    if (ImGui::Button("End Turn")) {
        unit.EndTurn();
    }
}

template <>
void WorldService::DrawComponent<BoundsComponent>(Entity entity, Elysium::World* world) {
    auto& bounds = world->GetComponent<BoundsComponent>(entity);

    FIELD_LABEL("X: ")
    ImGui::Text("%.1f", bounds.bounds.x);
    FIELD_LABEL("Y: ")
    ImGui::Text("%.1f", bounds.bounds.y);
    FIELD_LABEL("Width: ")
    ImGui::Text("%.1f", bounds.bounds.width);
    FIELD_LABEL("Height: ")
    ImGui::Text("%.1f", bounds.bounds.height);

    FIELD_LABEL("Is Dragging: ")
    ImGui::Checkbox("##IsDragging", &bounds.isDragging);

    float color[4] = {bounds.debugColor.r / 255.0f, bounds.debugColor.g / 255.0f,
                      bounds.debugColor.b / 255.0f, bounds.debugColor.a / 255.0f};
    FIELD_LABEL("Debug Color: ")
    if (ImGui::ColorEdit4("##DebugColor", color)) {
        bounds.debugColor = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255),
                             (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255)};
    }

    ImGui::Separator();
    ImGui::TextDisabled("Bounds are computed automatically by RenderSystem");
}

template <>
void WorldService::DrawComponent<ScriptComponent>(Entity entity, Elysium::World* world) {
    auto& script = world->GetComponent<ScriptComponent>(entity);

    // Script asset picker
    FIELD_LABEL("Script Asset: ")
    auto& assetService = Elysium::Application::GetInstance().GetService<AssetService>();
    const auto& allAssets = assetService.GetAllAssets();

    std::vector<std::string> scriptAssetNames;
    scriptAssetNames.push_back("<None>");

    for (const auto& [name, asset] : allAssets) {
        if (asset.GetType() == AssetType::SCRIPT && asset.IsLoaded()) {
            scriptAssetNames.push_back(name);
        }
    }

    std::string currentScript = script.scriptName.empty() ? "<None>" : script.scriptName;
    int currentIndex = 0;
    for (size_t i = 0; i < scriptAssetNames.size(); ++i) {
        if (scriptAssetNames[i] == currentScript) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    std::string comboId = "##ScriptAsset_" + std::to_string(entity);
    if (ImGui::BeginCombo(comboId.c_str(), currentScript.c_str())) {
        for (size_t i = 0; i < scriptAssetNames.size(); ++i) {
            bool isSelected = (currentIndex == static_cast<int>(i));
            if (ImGui::Selectable(scriptAssetNames[i].c_str(), isSelected)) {
                script.scriptName = (i == 0) ? "" : scriptAssetNames[i] + ".lua";
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    FIELD_LABEL("Is Active: ")
    std::string activeId = "##ScriptActive_" + std::to_string(entity);
    ImGui::Checkbox(activeId.c_str(), &script.isActive);

    if (!script.scriptName.empty() && script.isActive) {
        if (ImGui::CollapsingHeader("Script Data")) {
            auto& scriptService = Elysium::Application::GetInstance().GetService<ScriptService>();
            scriptService.InspectEntityScript(entity);
        }
    }
}

}  // namespace Elysium::Services

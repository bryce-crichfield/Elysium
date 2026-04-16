#include "Services/EditorService.h"
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/ComponentRegistry.h"
#include "Core/Components.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Core/SceneLayer.h"
#include "Services/SceneService.h"
#include "Components/CameraComponent.h"
#include "Components/CircleComponent.h"
#include "Components/LayerComponent.h"
#include "Components/LightComponent.h"
#include "Components/PositionComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/ScaleComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TextComponent.h"
#include "Components/TileComponent.h"
#include "raylib.h"
#include "raymath.h"
#include <limits>

namespace Elysium::Services {

EditorService::EditorService() {
    name_ = "EditorService";
}

void EditorService::Initialize() {
    RegisterComponentTypes();
}

void EditorService::Shutdown() {}

// =============================================================================
// Service accessors
// =============================================================================

Elysium::World* EditorService::GetWorld() const {
    auto* scene = GetTopScene();
    return scene ? scene->GetWorld() : nullptr;
}

Elysium::Scene* EditorService::GetTopScene() const {
    auto& app = Elysium::Application::GetInstance();
    auto& sceneService = app.GetService<SceneService>();
    return sceneService.GetTopScene();
}

// =============================================================================
// Update — runs each frame, owns all viewport interaction when in Editor mode
// =============================================================================

void EditorService::Update(float deltaTime) {
    Profile;

    if (Elysium::Application::GetInstance().GetMode() != AppMode::Editor)
        return;

    Elysium::World* world = GetWorld();
    Elysium::Scene* scene = GetTopScene();
    if (!world) return;

    ComputeSelectedAABB(world, scene);
    ProcessViewportInput(world, scene, deltaTime);
}

// =============================================================================
// AABB for the selected entity only (used by overlay and drag hit-test)
// =============================================================================

void EditorService::ComputeSelectedAABB(Elysium::World* world, Elysium::Scene* scene) {
    selectedAABBValid_ = false;

    if (selectedEntity_ == INVALID_ENTITY) return;
    if (!world->HasComponent<PositionComponent>(selectedEntity_)) return;
    if (world->HasComponent<CameraComponent>(selectedEntity_)) return;

    auto& pos = world->GetComponent<PositionComponent>(selectedEntity_);

    Vector2 min = { std::numeric_limits<float>::max(),    std::numeric_limits<float>::max()    };
    Vector2 max = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
    bool found  = false;

    float sx = 1.0f, sy = 1.0f;
    if (world->HasComponent<ScaleComponent>(selectedEntity_)) {
        auto& sc = world->GetComponent<ScaleComponent>(selectedEntity_);
        sx = sc.x; sy = sc.y;
    }

    bool isScreen2D = false;
    if (scene) {
        std::string layerName = "default";
        if (world->HasComponent<LayerComponent>(selectedEntity_))
            layerName = world->GetComponent<LayerComponent>(selectedEntity_).name;
        SceneLayer* layer = scene->GetLayer(layerName);
        isScreen2D = layer && layer->space == SceneLayerSpace::Screen2D;
    }

    if (world->HasComponent<RectangleComponent>(selectedEntity_)) {
        auto& r = world->GetComponent<RectangleComponent>(selectedEntity_);
        if (isScreen2D) {
            min.x = std::min(min.x, pos.x);
            min.y = std::min(min.y, pos.y);
            max.x = std::max(max.x, pos.x + r.width  * sx);
            max.y = std::max(max.y, pos.y + r.height * sy);
        } else {
            float hw = r.width  * sx * 0.5f;
            float hh = r.height * sy * 0.5f;
            min.x = std::min(min.x, pos.x - hw); min.y = std::min(min.y, pos.y - hh);
            max.x = std::max(max.x, pos.x + hw); max.y = std::max(max.y, pos.y + hh);
        }
        found = true;
    }

    if (world->HasComponent<CircleComponent>(selectedEntity_)) {
        auto& c = world->GetComponent<CircleComponent>(selectedEntity_);
        min.x = std::min(min.x, pos.x - c.radius); min.y = std::min(min.y, pos.y - c.radius);
        max.x = std::max(max.x, pos.x + c.radius); max.y = std::max(max.y, pos.y + c.radius);
        found = true;
    }

    if (world->HasComponent<TextComponent>(selectedEntity_)) {
        auto& t = world->GetComponent<TextComponent>(selectedEntity_);
        if (!(isScreen2D && world->HasComponent<RectangleComponent>(selectedEntity_))) {
            float avgScale = (sx + sy) * 0.5f;
            int scaledSize = (int)(t.fontSize * avgScale);
            int textWidth  = MeasureText(t.content.c_str(), scaledSize);
            float hw = textWidth  * 0.5f;
            float hh = scaledSize * 0.5f;
            min.x = std::min(min.x, pos.x - hw); min.y = std::min(min.y, pos.y - hh);
            max.x = std::max(max.x, pos.x + hw); max.y = std::max(max.y, pos.y + hh);
            found = true;
        }
    }

    if (world->HasComponent<SpriteComponent>(selectedEntity_)) {
        float hw = 16.0f * sx, hh = 16.0f * sy;
        min.x = std::min(min.x, pos.x - hw); min.y = std::min(min.y, pos.y - hh);
        max.x = std::max(max.x, pos.x + hw); max.y = std::max(max.y, pos.y + hh);
        found = true;
    }

    if (world->HasComponent<TileComponent>(selectedEntity_)) {
        auto& tc = world->GetComponent<TileComponent>(selectedEntity_);
        float hw = tc.tileWidth  * sx * 0.5f;
        float hh = tc.tileHeight * sy * 0.5f;
        min.x = std::min(min.x, pos.x - hw); min.y = std::min(min.y, pos.y - hh);
        max.x = std::max(max.x, pos.x + hw); max.y = std::max(max.y, pos.y + hh);
        found = true;
    }

    if (world->HasComponent<LightComponent>(selectedEntity_)) {
        const float kLightGizmoHalf = 10.0f;
        min.x = std::min(min.x, pos.x - kLightGizmoHalf);
        min.y = std::min(min.y, pos.y - kLightGizmoHalf);
        max.x = std::max(max.x, pos.x + kLightGizmoHalf);
        max.y = std::max(max.y, pos.y + kLightGizmoHalf);
        found = true;
    }

    if (found) {
        selectedAABB_      = { min, max, isScreen2D };
        selectedAABBValid_ = true;
    }
}

// =============================================================================
// Camera helpers
// =============================================================================

Entity EditorService::GetCameraEntity(Elysium::World* world) const {
    Entity result = INVALID_ENTITY;
    world->Query<CameraComponent>([&](Entity entity, auto&) {
        result = entity;
    });
    return result;
}

Vector2 EditorService::FbToWorld(Vector2 fbPos, Elysium::World* world, Entity cameraEntity) const {
    if (cameraEntity == INVALID_ENTITY || !world->HasComponent<PositionComponent>(cameraEntity))
        return fbPos;

    auto& camPos = world->GetComponent<PositionComponent>(cameraEntity);
    auto& cam    = world->GetComponent<CameraComponent>(cameraEntity);
    float zoom   = cam.zoom > 0.0f ? cam.zoom : 1.0f;
    float cx     = cam.viewport.width  * 0.5f;
    float cy     = cam.viewport.height * 0.5f;

    return {
        (fbPos.x - cx) / zoom + camPos.x,
        (fbPos.y - cy) / zoom + camPos.y
    };
}

// =============================================================================
// Viewport input (polled directly — SceneService is paused in Editor mode)
// =============================================================================

void EditorService::ProcessViewportInput(Elysium::World* world, Elysium::Scene* scene, float deltaTime) {
    auto& app          = Elysium::Application::GetInstance();
    auto& sceneService = app.GetService<SceneService>();
    const auto& vp     = sceneService.GetViewportRect();
    auto& fb           = sceneService.GetFramebuffer();
    float fbW = (float)fb.texture.width;
    float fbH = (float)fb.texture.height;

    if (vp.width <= 0.0f || vp.height <= 0.0f) return;

    Vector2 mousePos = GetMousePosition();
    bool inViewport  = CheckCollisionPointRec(mousePos, vp);

    // Screen → framebuffer position
    auto toFb = [&](Vector2 p) -> Vector2 {
        return {
            (p.x - vp.x) / vp.width  * fbW,
            (p.y - vp.y) / vp.height * fbH
        };
    };

    // Screen delta → framebuffer delta
    Vector2 screenDelta = GetMouseDelta();
    Vector2 fbDelta = {
        screenDelta.x / vp.width  * fbW,
        screenDelta.y / vp.height * fbH
    };

    Vector2 fbPos = toFb(mousePos);

    // ---- Middle-mouse pan ----
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && inViewport) isPanning_ = true;
    if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE))               isPanning_ = false;

    if (isPanning_) {
        Entity camEntity = GetCameraEntity(world);
        if (camEntity != INVALID_ENTITY && world->HasComponent<PositionComponent>(camEntity)) {
            auto& camPos = world->GetComponent<PositionComponent>(camEntity);
            auto& cam    = world->GetComponent<CameraComponent>(camEntity);
            float zoom   = cam.zoom > 0.0f ? cam.zoom : 1.0f;
            camPos.x -= fbDelta.x / zoom;
            camPos.y -= fbDelta.y / zoom;
        }
        return;
    }

    // ---- Scroll wheel zoom (anchored to cursor) ----
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && inViewport) {
        Entity camEntity = GetCameraEntity(world);
        if (camEntity != INVALID_ENTITY &&
            world->HasComponent<CameraComponent>(camEntity) &&
            world->HasComponent<PositionComponent>(camEntity)) {
            auto& cam    = world->GetComponent<CameraComponent>(camEntity);
            auto& camPos = world->GetComponent<PositionComponent>(camEntity);
            float oldZoom = cam.zoom > 0.0f ? cam.zoom : 1.0f;
            float newZoom = Clamp(oldZoom * (wheel > 0.0f ? 1.15f : (1.0f / 1.15f)), 0.1f, 20.0f);
            float cx = cam.viewport.width  * 0.5f;
            float cy = cam.viewport.height * 0.5f;
            camPos.x += (fbPos.x - cx) * (1.0f / oldZoom - 1.0f / newZoom);
            camPos.y += (fbPos.y - cy) * (1.0f / oldZoom - 1.0f / newZoom);
            cam.zoom = newZoom;
        }
    }

    // ---- WASD / arrow-key pan (only while mouse is over the viewport) ----
    if (inViewport) {
        float panX = 0.0f, panY = 0.0f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    panY -= 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  panY += 1.0f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  panX -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) panX += 1.0f;
        if (panX != 0.0f || panY != 0.0f) {
            Entity camEntity = GetCameraEntity(world);
            if (camEntity != INVALID_ENTITY &&
                world->HasComponent<PositionComponent>(camEntity) &&
                world->HasComponent<CameraComponent>(camEntity)) {
                auto& camPos = world->GetComponent<PositionComponent>(camEntity);
                auto& cam    = world->GetComponent<CameraComponent>(camEntity);
                float zoom   = cam.zoom > 0.0f ? cam.zoom : 1.0f;
                float len    = sqrtf(panX * panX + panY * panY);
                const float panSpeed = 600.0f;
                camPos.x += (panX / len) * panSpeed * deltaTime / zoom;
                camPos.y += (panY / len) * panSpeed * deltaTime / zoom;
            }
        }
    }

    // ---- Left press: start drag if click is within the selected entity's AABB ----
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inViewport) {
        pressPos_   = fbPos;
        hasDragged_ = false;
        isDragging_ = false;

        if (selectedEntity_ != INVALID_ENTITY && selectedAABBValid_) {
            Entity camEntity = GetCameraEntity(world);
            Vector2 testPos  = selectedAABB_.isScreenSpace
                                   ? fbPos
                                   : FbToWorld(fbPos, world, camEntity);
            if (testPos.x >= selectedAABB_.min.x && testPos.x <= selectedAABB_.max.x &&
                testPos.y >= selectedAABB_.min.y && testPos.y <= selectedAABB_.max.y) {
                isDragging_ = true;
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        isDragging_ = false;
    }

    // ---- Drag: move selected entity ----
    constexpr float kDragThreshold = 2.0f;  // framebuffer pixels
    if (isDragging_ && selectedEntity_ != INVALID_ENTITY &&
        world->HasComponent<PositionComponent>(selectedEntity_)) {

        float moved = Vector2Length(Vector2Subtract(fbPos, pressPos_));
        if (moved >= kDragThreshold) hasDragged_ = true;
        if (!hasDragged_) return;

        // Tiles are part of a tilemap grid and shouldn't be freely repositioned
        if (world->HasComponent<TileComponent>(selectedEntity_)) return;

        bool isScreenSpace = false;
        if (scene) {
            std::string layerName = "default";
            if (world->HasComponent<LayerComponent>(selectedEntity_))
                layerName = world->GetComponent<LayerComponent>(selectedEntity_).name;
            SceneLayer* layer = scene->GetLayer(layerName);
            isScreenSpace = layer && layer->space == SceneLayerSpace::Screen2D;
        }

        auto& pos = world->GetComponent<PositionComponent>(selectedEntity_);

        if (isScreenSpace) {
            pos.x += fbDelta.x;
            pos.y += fbDelta.y;
        } else {
            Entity camEntity = GetCameraEntity(world);
            float zoom = 1.0f;
            if (camEntity != INVALID_ENTITY)
                zoom = world->GetComponent<CameraComponent>(camEntity).zoom;
            if (zoom <= 0.0f) zoom = 1.0f;
            pos.x += fbDelta.x / zoom;
            pos.y += fbDelta.y / zoom;
        }
    }
}

// =============================================================================
// Overlay rendering (call inside BeginTextureMode on the scene framebuffer)
// =============================================================================

void EditorService::RenderOverlay(float fbW, float fbH) {
    if (selectedEntity_ == INVALID_ENTITY || !selectedAABBValid_) return;

    const EntityAABB& aabb = selectedAABB_;

    Vector2 fbMin, fbMax;

    if (aabb.isScreenSpace) {
        fbMin = aabb.min;
        fbMax = aabb.max;
    } else {
        Elysium::World* world = GetWorld();
        if (!world) return;

        Entity camEntity = GetCameraEntity(world);
        if (camEntity == INVALID_ENTITY) return;

        auto& camPos = world->GetComponent<PositionComponent>(camEntity);
        auto& cam    = world->GetComponent<CameraComponent>(camEntity);
        float zoom   = cam.zoom > 0.0f ? cam.zoom : 1.0f;
        float cx     = cam.viewport.width  * 0.5f;
        float cy     = cam.viewport.height * 0.5f;

        fbMin = {
            (aabb.min.x - camPos.x) * zoom + cx,
            (aabb.min.y - camPos.y) * zoom + cy
        };
        fbMax = {
            (aabb.max.x - camPos.x) * zoom + cx,
            (aabb.max.y - camPos.y) * zoom + cy
        };
    }

    float w = fbMax.x - fbMin.x;
    float h = fbMax.y - fbMin.y;
    if (w <= 0 || h <= 0) return;

    const Color outlineColor = { 0, 230, 90, 255 };

    DrawRectangleLinesEx({ fbMin.x, fbMin.y, w, h }, 1.5f, outlineColor);

    const float hs = 5.0f;
    auto drawHandle = [&](float x, float y) {
        DrawRectangleV({ x - hs * 0.5f, y - hs * 0.5f }, { hs, hs }, outlineColor);
    };
    drawHandle(fbMin.x, fbMin.y);
    drawHandle(fbMax.x, fbMin.y);
    drawHandle(fbMin.x, fbMax.y);
    drawHandle(fbMax.x, fbMax.y);
    drawHandle((fbMin.x + fbMax.x) * 0.5f, fbMin.y);
    drawHandle((fbMin.x + fbMax.x) * 0.5f, fbMax.y);
    drawHandle(fbMin.x, (fbMin.y + fbMax.y) * 0.5f);
    drawHandle(fbMax.x, (fbMin.y + fbMax.y) * 0.5f);
}

// =============================================================================
// Component registration
// =============================================================================

void EditorService::RegisterComponentTypes() {
    const auto& inspectors = ComponentRegistry::Instance().GetInspectors();
    for (const auto& [name, inspectorFunc] : inspectors) {
        ComponentPlaceholder placeholder;
        placeholder.name    = name;
        placeholder.drawFunc = [inspectorFunc](Entity e, Elysium::World* w) {
            inspectorFunc(w, e);
        };

        if (auto* access = ComponentRegistry::Instance().GetLuaAccess(name)) {
            placeholder.hasComponentFunc    = [access](Entity e, Elysium::World* w) { return access->has(w, e); };
            placeholder.addComponentFunc    = [access](Entity e, Elysium::World* w) { access->add(w, e); };
            placeholder.removeComponentFunc = [access](Entity e, Elysium::World* w) { access->remove(w, e); };
            placeholder.resetComponentFunc  = [access](Entity e, Elysium::World* w) {
                access->remove(w, e);
                access->add(w, e);
            };
        }

        componentPlaceholders_.push_back(placeholder);
    }

    std::sort(componentPlaceholders_.begin(), componentPlaceholders_.end(),
              [](const ComponentPlaceholder& a, const ComponentPlaceholder& b) {
                  return a.name < b.name;
              });
}

}  // namespace Elysium::Services

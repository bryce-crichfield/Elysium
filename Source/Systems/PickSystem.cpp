#include "Systems/PickSystem.h"
#include <unordered_map>
#include "Core/Application.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Services/LogService.h"
#include "raymath.h"
#include "rlgl.h"

namespace Elysium::Systems {

PickSystem::PickSystem(Context context) : System(context) {
    world->AddWorldListener(this);
}

PickSystem::~PickSystem() {
    world->RemoveWorldListener(this);
}

void PickSystem::Update(float deltaTime) {
    // Selection logic is event-driven
}

void PickSystem::Draw() {
    if (!isBoxSelecting_) {
        return;
    }

    // Draw selection box in world space
    Entity cameraEntity = 0;
    CameraComponent* camera = nullptr;
    Vector2 cameraPos = {0, 0};

    world->Query<CameraComponent, PositionComponent>([&](Entity entity, auto& cam, auto& pos) {
        if (cam.isVisible) {
            cameraEntity = entity;
            camera = &cam;
            cameraPos = {pos.x, pos.y};
        }
    });

    if (!camera) {
        return;
    }

    // Apply camera transform
    Vector2 viewportCenter = {
        camera->viewport.width * 0.5f,
        camera->viewport.height * 0.5f};

    Matrix centerTranslation = MatrixTranslate(viewportCenter.x, viewportCenter.y, 0);
    Matrix scale = MatrixScale(camera->zoom, camera->zoom, 1.0f);
    Matrix cameraTranslation = MatrixTranslate(-cameraPos.x, -cameraPos.y, 0);
    Matrix transform = MatrixMultiply(MatrixMultiply(cameraTranslation, scale), centerTranslation);

    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(transform));

    Rectangle box = GetSelectionBox();

    // Draw filled semi-transparent box
    Color fillColor = {0, 120, 215, 50};
    DrawRectangleRec(box, fillColor);

    // Draw border
    Color borderColor = {0, 120, 215, 200};
    DrawRectangleLinesEx(box, 1.0f, borderColor);

    rlPopMatrix();
}

void PickSystem::OnEvent(Event& event) {
    DispatchMouseEvent(event);
}

void PickSystem::OnMouseButtonPressed(MouseButtonPressedEvent& event) {
    if (event.GetButton() != MOUSE_LEFT_BUTTON) {
        return;
    }

    Vector2 fbPos = event.GetPosition();

    // First check screen-space UI elements (they take priority)
    Entity screenEntity = FindScreenSpaceEntityAtPoint(fbPos);
    if (screenEntity != 0) {
        // Fire pick event on screen-space entity through scene event system
        pressedEntity_ = screenEntity;
        PickEvent pickEvent(PickEvent::Type::PRESS, fbPos, screenEntity);
        GetScene()->OnEvent(pickEvent);
        event.handled = true;
        LOG_INFOF("PickSystem", "Screen-space entity %zu pressed at (%.1f, %.1f)", screenEntity, fbPos.x, fbPos.y);
        return;
    }

    // Fall back to world-space picking
    Vector2 worldPos = FramebufferToWorld(fbPos);
    Entity clickedEntity = FindEntityAtPoint(worldPos);

    if (clickedEntity != 0) {
        // Clicked on an entity
        bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool isCurrentlySelected = world->HasComponent<SelectionComponent>(clickedEntity);

        if (shiftHeld) {
            // Toggle selection
            if (isCurrentlySelected) {
                world->RemoveComponent<SelectionComponent>(clickedEntity);
            } else {
                world->AddComponent<SelectionComponent>(clickedEntity, SelectionComponent{});
            }
        } else {
            // Single select - deselect all others first
            DeselectAll();
            world->AddComponent<SelectionComponent>(clickedEntity, SelectionComponent{});
        }

        // Fire pick event through scene event system
        pressedEntity_ = clickedEntity;
        PickEvent pickEvent(PickEvent::Type::PRESS, worldPos, clickedEntity);
        GetScene()->OnEvent(pickEvent);

        LOG_INFOF("PickSystem", "Selected entity %zu at (%.1f, %.1f)", clickedEntity, worldPos.x, worldPos.y);
    } else {
        // Clicked on empty space - start box selection
        bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (!shiftHeld) {
            DeselectAll();
        }

        isBoxSelecting_ = true;
        boxStart_ = worldPos;
        boxCurrent_ = worldPos;
        pressedEntity_ = 0;

        LOG_INFOF("PickSystem", "Started box selection at (%.1f, %.1f)", worldPos.x, worldPos.y);
    }
}

void PickSystem::OnMouseMoved(MouseMovedEvent& event) {
    if (!isBoxSelecting_) {
        return;
    }

    Vector2 fbPos = event.GetPosition();
    boxCurrent_ = FramebufferToWorld(fbPos);
}

void PickSystem::OnMouseButtonReleased(MouseButtonReleasedEvent& event) {
    if (event.GetButton() != MOUSE_LEFT_BUTTON) {
        return;
    }

    Vector2 fbPos = event.GetPosition();

    // Fire release event on the entity that was pressed
    if (pressedEntity_ != 0) {
        PickEvent pickEvent(PickEvent::Type::RELEASE, fbPos, pressedEntity_);
        GetScene()->OnEvent(pickEvent);
        LOG_INFOF("PickSystem", "Entity %zu released", pressedEntity_);
        pressedEntity_ = 0;
        return;
    }

    if (isBoxSelecting_) {
        // Complete box selection
        Rectangle box = GetSelectionBox();

        // Only select if box has meaningful size
        if (box.width > 5.0f || box.height > 5.0f) {
            world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
                if (BoundsIntersectsBox(bounds.bounds, box)) {
                    if (!world->HasComponent<SelectionComponent>(entity)) {
                        world->AddComponent<SelectionComponent>(entity, SelectionComponent{});
                    }
                }
            });
        }

        isBoxSelecting_ = false;
        LOG_INFO("PickSystem", "Completed box selection");
    }
}

bool PickSystem::IsPointInBounds(Vector2 point, const Rectangle& bounds) {
    return CheckCollisionPointRec(point, bounds);
}

bool PickSystem::BoundsIntersectsBox(const Rectangle& bounds, const Rectangle& box) {
    return CheckCollisionRecs(bounds, box);
}

Vector2 PickSystem::FramebufferToWorld(Vector2 fbPos) {
    Entity cameraEntity = 0;
    CameraComponent* camera = nullptr;
    Vector2 cameraPos = {0, 0};

    world->Query<CameraComponent, PositionComponent>([&](Entity entity, auto& cam, auto& pos) {
        if (cam.isVisible) {
            cameraEntity = entity;
            camera = &cam;
            cameraPos = {pos.x, pos.y};
        }
    });

    if (!camera) {
        return fbPos;
    }

    Vector2 viewportCenter = {
        camera->viewport.width * 0.5f,
        camera->viewport.height * 0.5f};

    Vector2 centered = {
        fbPos.x - viewportCenter.x,
        fbPos.y - viewportCenter.y};

    Vector2 unscaled = {
        centered.x / camera->zoom,
        centered.y / camera->zoom};

    Vector2 worldPos = {
        unscaled.x + cameraPos.x,
        unscaled.y + cameraPos.y};

    return worldPos;
}

Rectangle PickSystem::GetSelectionBox() const {
    float x = (boxStart_.x < boxCurrent_.x) ? boxStart_.x : boxCurrent_.x;
    float y = (boxStart_.y < boxCurrent_.y) ? boxStart_.y : boxCurrent_.y;
    float w = fabsf(boxCurrent_.x - boxStart_.x);
    float h = fabsf(boxCurrent_.y - boxStart_.y);
    return {x, y, w, h};
}

void PickSystem::DeselectAll() {
    std::vector<Entity> toDeselect;
    world->Query<SelectionComponent>([&](Entity entity, auto& sel) {
        toDeselect.push_back(entity);
    });
    for (Entity e : toDeselect) {
        world->RemoveComponent<SelectionComponent>(e);
    }
}

Entity PickSystem::FindEntityAtPoint(Vector2 worldPos) {
    Entity foundEntity = 0;

    world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
        if (IsPointInBounds(worldPos, bounds.bounds)) {
            foundEntity = entity;
        }
    });

    return foundEntity;
}

Entity PickSystem::FindScreenSpaceEntityAtPoint(Vector2 screenPos) {
    Entity foundEntity = 0;

    // Build layer lookup for screen-space detection
    std::unordered_map<std::string, bool> screenSpaceLayers;
    world->Query<LayerComponent>([&](Entity entity, auto& layer) {
        screenSpaceLayers[layer.name] = (layer.space == LayerComponent::Space::Screen);
    });

    // Check entities with bounds - prefer screen-space entities (higher z-order typically)
    world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
        // Get the layer name from the entity's renderable component
        std::string layerName = "default";
        if (world->HasComponent<RectangleComponent>(entity)) {
            layerName = world->GetComponent<RectangleComponent>(entity).layerName;
        } else if (world->HasComponent<TextComponent>(entity)) {
            layerName = world->GetComponent<TextComponent>(entity).layerName;
        } else if (world->HasComponent<CircleComponent>(entity)) {
            layerName = world->GetComponent<CircleComponent>(entity).layerName;
        }

        // Check if this is a screen-space layer
        auto it = screenSpaceLayers.find(layerName);
        if (it != screenSpaceLayers.end() && it->second) {
            // Screen-space entity - check against screen coordinates
            if (IsPointInBounds(screenPos, bounds.bounds)) {
                foundEntity = entity;
            }
        }
    });

    return foundEntity;
}

}  // namespace Elysium::Systems

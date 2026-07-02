#include "Systems/DebugSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Scene.h"
#include "Core/Application.h"
#include "Components/LayerComponent.h"
#include "Components/BoundsComponent.h"
#include "Components/CameraComponent.h"
#include "Components/DebugComponent.h"
#include "Components/NameComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/CircleComponent.h"
#include "Components/TextComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/LightComponent.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <limits>

namespace Elysium::Systems {

DebugSystem::DebugSystem(Context context) : System(context) {
    world->AddWorldListener(this);

    // Attach DebugComponent to all existing entities
    world->Query<NameComponent>([&](Entity entity, auto& name) {
        if (!world->HasComponent<DebugComponent>(entity)) {
            world->AddComponent<DebugComponent>(entity, DebugComponent{});
        }
    });
}

DebugSystem::~DebugSystem() {
    // Remove DebugComponent from all entities
    std::vector<Entity> toRemove;
    world->Query<DebugComponent>([&](Entity entity, auto& debug) {
        toRemove.push_back(entity);
    });
    for (Entity e : toRemove) {
        world->RemoveComponent<DebugComponent>(e);
    }

    world->RemoveWorldListener(this);
}

void DebugSystem::Update(float deltaTime) {
    world->Query<DebugComponent, TransformComponent>([&](Entity entity, auto& debug, auto& transform) {
        auto debuggables = GetDebuggables(entity);
        if (debuggables.empty()) return;

        Vector2 min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        Vector2 max = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        bool foundAny = false;

        Vector2 pos = {transform.worldX, transform.worldY};
        float scaleX = transform.worldScaleX;
        float scaleY = transform.worldScaleY;

        // Resolve this entity's layer space once, used by per-component AABB logic below.
        bool isScreen2D = false;
        if (scene) {
            std::string layerName = "default";
            if (world->HasComponent<LayerComponent>(entity))
                layerName = world->GetComponent<LayerComponent>(entity).name;
            SceneLayer* layer = scene->GetLayer(layerName);
            isScreen2D = layer && layer->space == SceneLayerSpace::Screen2D;
        }

        for (const auto& item : debuggables) {
            std::visit([&](const auto& component) {
                using T = std::decay_t<decltype(component)>;

                if constexpr (std::is_same_v<T, RectangleComponent>) {
                    if (isScreen2D) {
                        // Screen2D: position is top-left corner, extend to bottom-right.
                        min.x = std::min(min.x, pos.x);
                        min.y = std::min(min.y, pos.y);
                        max.x = std::max(max.x, pos.x + component.width);
                        max.y = std::max(max.y, pos.y + component.height);
                        foundAny = true;
                        return;
                    }
                    // World2D: position is center.
                    float hw = component.width  * 0.5f;
                    float hh = component.height * 0.5f;
                    min.x = std::min(min.x, pos.x - hw); min.y = std::min(min.y, pos.y - hh);
                    max.x = std::max(max.x, pos.x + hw); max.y = std::max(max.y, pos.y + hh);
                    foundAny = true;
                } else if constexpr (std::is_same_v<T, CircleComponent>) {
                    float r = component.radius;
                    min.x = std::min(min.x, pos.x - r); min.y = std::min(min.y, pos.y - r);
                    max.x = std::max(max.x, pos.x + r); max.y = std::max(max.y, pos.y + r);
                    foundAny = true;
                } else if constexpr (std::is_same_v<T, TextComponent>) {
                    // In Screen2D, if the entity has a rect the text is centered within it —
                    // the rect AABB already covers it, so skip the separate text contribution.
                    if (isScreen2D && world->HasComponent<RectangleComponent>(entity))
                        return;
                    float avgScale = (scaleX + scaleY) * 0.5f;
                    int scaledFontSize = (int)(component.fontSize * avgScale);
                    int textWidth = MeasureText(component.content.c_str(), scaledFontSize);
                    float hw = textWidth * 0.5f;
                    float hh = scaledFontSize * 0.5f;
                    min.x = std::min(min.x, pos.x - hw); min.y = std::min(min.y, pos.y - hh);
                    max.x = std::max(max.x, pos.x + hw); max.y = std::max(max.y, pos.y + hh);
                    foundAny = true;
                } else if constexpr (std::is_same_v<T, SpriteComponent>) {
                    // TODO: Reimplement sprite size calculation
                } else if constexpr (std::is_same_v<T, LightComponent>) {
                    float r = component.radius;
                    min.x = std::min(min.x, pos.x - r); min.y = std::min(min.y, pos.y - r);
                    max.x = std::max(max.x, pos.x + r); max.y = std::max(max.y, pos.y + r);
                    foundAny = true;
                }
            }, item);
        }

        if (foundAny) {
            debug.aabbMin = min;
            debug.aabbMax = max;
        }
    });
}

std::vector<Debuggable> DebugSystem::GetDebuggables(Entity entity) {
    std::vector<Debuggable> debuggables;
    if (world->HasComponent<RectangleComponent>(entity)) {
        debuggables.push_back(world->GetComponent<RectangleComponent>(entity));
    }
    if (world->HasComponent<CircleComponent>(entity)) {
        debuggables.push_back(world->GetComponent<CircleComponent>(entity));
    }
    if (world->HasComponent<TextComponent>(entity)) {
        debuggables.push_back(world->GetComponent<TextComponent>(entity));
    }
    if (world->HasComponent<SpriteComponent>(entity)) {
        debuggables.push_back(world->GetComponent<SpriteComponent>(entity));
    }
    if (world->HasComponent<LightComponent>(entity)) {
        debuggables.push_back(world->GetComponent<LightComponent>(entity));
    }
    return debuggables;
}

void DebugSystem::OnEntityCreated(Entity entity) {
    // Deferred: component pools may not be ready yet during creation.
}

void DebugSystem::OnEvent(Event& event) {
    DispatchMouseEvent(event);
}

void DebugSystem::OnMouseButtonPressed(MouseButtonPressedEvent& event) {
    if (event.GetButton() == MOUSE_BUTTON_MIDDLE) {
        isPanning_ = true;
        event.handled = true;
        return;
    }

    if (event.GetButton() != MOUSE_LEFT_BUTTON) {
        return;
    }

    Vector2 fbPos = event.GetPosition();
    Entity clicked = FindEntityAtPoint(fbPos);

    // If we clicked on the already selected entity, just start dragging
    if (clicked != 0 && clicked == selectedEntity_) {
        isDragging_ = true;
        event.handled = true;
        return;
    }

    // Clear previous selection
    if (selectedEntity_ != 0 && world->HasComponent<DebugComponent>(selectedEntity_)) {
        world->GetComponent<DebugComponent>(selectedEntity_).isSelected = false;
    }

    selectedEntity_ = clicked;
    isDragging_ = false; // Reset drag state on new selection

    if (clicked != 0) {
        // Ensure it has a DebugComponent
        if (!world->HasComponent<DebugComponent>(clicked)) {
            world->AddComponent<DebugComponent>(clicked, DebugComponent{});
        }
        world->GetComponent<DebugComponent>(clicked).isSelected = true;
        isDragging_ = true; // Start dragging immediately on selection
        event.handled = true;
    }
}

void DebugSystem::OnMouseButtonReleased(MouseButtonReleasedEvent& event) {
    if (event.GetButton() == MOUSE_LEFT_BUTTON) {
        isDragging_ = false;
    } else if (event.GetButton() == MOUSE_BUTTON_MIDDLE) {
        isPanning_ = false;
    }
}

void DebugSystem::OnMouseMoved(MouseMovedEvent& event) {
    Vector2 delta = event.GetDelta();

    if (isPanning_) {
        Entity cameraEntity = INVALID_ENTITY;
        world->Query<CameraComponent>([&](Entity entity, auto& camera) {
            cameraEntity = entity;
        });

        if (cameraEntity != INVALID_ENTITY && world->HasComponent<TransformComponent>(cameraEntity)) {
            auto& transform = world->GetComponent<TransformComponent>(cameraEntity);
            auto& cam = world->GetComponent<CameraComponent>(cameraEntity);
            float zoom = cam.zoom;
            if (zoom != 0.0f) {
                transform.localX -= delta.x / zoom;
                transform.localY -= delta.y / zoom;
            }
        }
        return;
    }

    if (!isDragging_ || selectedEntity_ == 0 || !world->HasComponent<TransformComponent>(selectedEntity_)) {
        return;
    }

    auto& pos = world->GetComponent<TransformComponent>(selectedEntity_);

    // Check layer to decide coordinate space
    std::string layerName = "default";
    if (world->HasComponent<LayerComponent>(selectedEntity_)) {
        layerName = world->GetComponent<LayerComponent>(selectedEntity_).name;
    }

    bool isScreenSpace = false;
    if (scene) {
        SceneLayer* layer = scene->GetLayer(layerName);
        if (layer && layer->space == SceneLayerSpace::Screen2D) {
            isScreenSpace = true;
        }
    }

    if (isScreenSpace) {
        pos.localX += delta.x;
        pos.localY += delta.y;
    } else {
        // Convert delta to world space
        // We need the camera zoom for scale. 
        // Rotation is ignored for now (assuming 0).
        
        Entity cameraEntity = INVALID_ENTITY;
        world->Query<CameraComponent>([&](Entity entity, auto& camera) {
            cameraEntity = entity;
        });

        float zoom = 1.0f;
        if (cameraEntity != INVALID_ENTITY) {
            zoom = world->GetComponent<CameraComponent>(cameraEntity).zoom;
        }
        
        // Avoid division by zero
        if (zoom != 0.0f) {
            pos.localX += delta.x / zoom;
            pos.localY += delta.y / zoom;
        }
    }
}

void DebugSystem::Draw() {

}

Vector2 DebugSystem::FramebufferToWorld(Vector2 fbPos) {
    // Get the camera entity (you'll need to store/query this)
    Entity cameraEntity = INVALID_ENTITY;

    world->Query<CameraComponent>([&](Entity entity, auto& camera) {
        cameraEntity = entity;
    });

    if (cameraEntity == INVALID_ENTITY) {
        return fbPos;  // No camera, assume screen space
    }

    auto& cameraTransform = world->GetComponent<TransformComponent>(cameraEntity);
    auto& camera = world->GetComponent<CameraComponent>(cameraEntity);

    // Reverse the viewport centering
    Vector2 centered = {
        fbPos.x - (camera.viewport.width * 0.5f),
        fbPos.y - (camera.viewport.height * 0.5f)};

    // Reverse the scale (divide by zoom)
    Vector2 unscaled = {
        centered.x / camera.zoom,
        centered.y / camera.zoom};

    // Reverse the camera translation (add camera position back)
    Vector2 worldPos = {
        unscaled.x + cameraTransform.worldX,
        unscaled.y + cameraTransform.worldY};

    return worldPos;
}

Entity DebugSystem::FindEntityAtPoint(Vector2 fbPos) {
    Entity foundEntity = 0;

    // Convert framebuffer position to world space once for world-space entities
    Vector2 worldPos = FramebufferToWorld(fbPos);

    world->Query<DebugComponent>([&](Entity entity, auto& debug) {
        Vector2 testPos = worldPos;

        std::string layerName = "default";
        if (world->HasComponent<LayerComponent>(entity)) {
            layerName = world->GetComponent<LayerComponent>(entity).name;
        }

        if (scene) {
            SceneLayer* layer = scene->GetLayer(layerName);
            if (layer && layer->space == SceneLayerSpace::Screen2D) {
                testPos = fbPos;
            }
        }

        if (testPos.x >= debug.aabbMin.x && testPos.x <= debug.aabbMax.x &&
            testPos.y >= debug.aabbMin.y && testPos.y <= debug.aabbMax.y) {
            foundEntity = entity;
        }
    });

    return foundEntity;
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::DebugSystem)

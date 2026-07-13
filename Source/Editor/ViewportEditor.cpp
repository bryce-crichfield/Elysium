#include "ViewportEditor.h"
#include <algorithm>
#include <cmath>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Core/World.h"
#include "Components/CameraComponent.h"
#include "Components/ParentComponent.h"
#include "Components/TransformComponent.h"
#include "Services/EditorService.h"
#include "Services/SceneService.h"
#include "Systems/RenderSystem.h"
#include "imgui.h"
#include "rlImGui.h"
#include "raymath.h"

namespace Elysium {

using namespace Services;
using Systems::CameraView;

ViewportEditor::ViewportEditor() : Editor("Game") {}

void ViewportEditor::Draw(Application& app) {
    Profile;

    auto& sceneService = app.GetService<SceneService>();
    auto& editorService = app.GetService<EditorService>();

    InitializeEditorCameraIfNeeded(editorService);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin(name_.c_str(), nullptr, ImGuiWindowFlags_NoCollapse)) {
        DrawToolbar(sceneService, editorService);

        // Grab content region position and size before drawing the image
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 avail = ImGui::GetContentRegionAvail();

        rlImGuiImageRenderTextureFit(&sceneService.GetFramebuffer(), true);

        // Snapshot the view that produced the currently-displayed framebuffer image
        // (i.e. before this frame's pan/zoom input is applied) so picking and gizmo
        // hit-testing line up with what's actually on screen right now.
        auto& editorCam = editorService.GetEditorCamera();
        const auto& config = app.GetConfig();
        CameraView view{
            editorCam.position,
            editorCam.zoom != 0.0f ? editorCam.zoom : 1.0f,
            Rectangle{0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight}
        };

        HandleEditorCameraInput(sceneService, editorService);
        HandleGizmoOrPick(sceneService, editorService, view);

        // Tell SceneService where the game viewport is on screen
        // rlImGuiImageRenderTextureFit centers the image maintaining aspect ratio
        float fbW = (float)sceneService.GetFramebuffer().texture.width;
        float fbH = (float)sceneService.GetFramebuffer().texture.height;
        float fbAspect = fbW / fbH;
        float regionAspect = avail.x / avail.y;

        float drawW, drawH;
        if (fbAspect > regionAspect) {
            drawW = avail.x;
            drawH = avail.x / fbAspect;
        } else {
            drawH = avail.y;
            drawW = avail.y * fbAspect;
        }
        float drawX = pos.x + (avail.x - drawW) * 0.5f;
        float drawY = pos.y + (avail.y - drawH) * 0.5f;

        sceneService.SetViewportRect(Rectangle{drawX, drawY, drawW, drawH});
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportEditor::DrawToolbar(SceneService& sceneService, EditorService& editor) {
    bool isPlaying = sceneService.IsPlaying();
    if (isPlaying) {
        if (ImGui::Button("Pause")) {
            sceneService.SetPlaying(false);
        }
    } else {
        if (ImGui::Button("Play")) {
            sceneService.SetPlaying(true);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled(isPlaying ? "Simulating" : "Paused");

    auto &camera = editor.GetEditorCamera();
    std::string positionText = "X: " + std::to_string(camera.position.x) + ", Y: " + std::to_string(camera.position.y);
    std::string zoomText = "Zoom: " + std::to_string(camera.zoom);

    ImGui::SameLine();
    ImGui::Text("%s | %s", positionText.c_str(), zoomText.c_str());
}

void ViewportEditor::InitializeEditorCameraIfNeeded(EditorService& editorService) {
    auto* world = editorService.GetWorld();
    if (world != lastWorld_) {
        lastWorld_ = world;
        editorService.GetEditorCamera().initialized = false;
    }

    auto& cam = editorService.GetEditorCamera();
    if (cam.initialized || !world) return;

    // Snap to the first real camera found so the editor doesn't open centered on the
    // origin; the user is then free to pan/zoom independently from there.
    world->Query<CameraComponent>([&](Entity entity, auto& camComp) {
        if (cam.initialized) return;
        cam.zoom = camComp.zoom;
        if (world->HasComponent<TransformComponent>(entity)) {
            auto& t = world->GetComponent<TransformComponent>(entity);
            cam.position = { t.worldX, t.worldY };
        }
        cam.initialized = true;
    });
}

void ViewportEditor::HandleEditorCameraInput(SceneService& sceneService, EditorService& editorService) {
    auto& cam = editorService.GetEditorCamera();
    bool hovered = ImGui::IsItemHovered();

    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        isPanningCamera_ = true;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        isPanningCamera_ = false;
    }

    if (isPanningCamera_) {
        Vector2 delta = GetMouseDelta();
        float zoom = cam.zoom != 0.0f ? cam.zoom : 1.0f;
        cam.position.x -= delta.x / zoom;
        cam.position.y -= delta.y / zoom;
    }

    if (hovered) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            // Same viewport-center/zoom math as RenderSystem::CalculateTransform's World2D
            // branch — find the world point under the cursor before changing zoom, then
            // re-solve the camera position so that same world point stays under the cursor.
            const auto& config = Application::GetInstance().GetConfig();
            Vector2 viewportCenter = { config.framebufferWidth * 0.5f, config.framebufferHeight * 0.5f };
            Vector2 mouseFbPos = sceneService.ScreenToFramebuffer(GetMousePosition());

            float oldZoom = cam.zoom != 0.0f ? cam.zoom : 1.0f;
            Vector2 worldUnderMouse = {
                (mouseFbPos.x - viewportCenter.x) / oldZoom + cam.position.x,
                (mouseFbPos.y - viewportCenter.y) / oldZoom + cam.position.y
            };

            float factor = 1.0f + wheel * 0.1f;
            float newZoom = std::clamp(cam.zoom * factor, 0.1f, 10.0f);
            cam.zoom = newZoom;

            cam.position.x = worldUnderMouse.x - (mouseFbPos.x - viewportCenter.x) / newZoom;
            cam.position.y = worldUnderMouse.y - (mouseFbPos.y - viewportCenter.y) / newZoom;
        }
    }
}

void ViewportEditor::HandleGizmoOrPick(SceneService& sceneService, EditorService& editorService, const CameraView& view) {
    auto* world = editorService.GetWorld();

    // Continue or end an in-progress drag. Either way, swallow this frame's click —
    // it belongs to the drag, not to re-picking.
    if (isDraggingGizmo_) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && world &&
            world->HasComponent<TransformComponent>(gizmoEntity_)) {
            Vector2 fbPos = sceneService.ScreenToFramebuffer(GetMousePosition());
            Vector2 fbDelta = { fbPos.x - lastGizmoFbPos_.x, fbPos.y - lastGizmoFbPos_.y };
            lastGizmoFbPos_ = fbPos;
            ApplyGizmoDrag(world, gizmoEntity_, view.zoom, fbDelta);
        } else {
            isDraggingGizmo_ = false;
        }
        return;
    }

    // Start a drag if this press landed on the single selected entity's move handle.
    auto* renderSystem = Systems::RenderSystem::GetCurrent();
    const auto& selected = editorService.GetSelectedEntities();
    if (selected.size() == 1 && world && renderSystem &&
        world->HasComponent<TransformComponent>(selected[0]) &&
        ImGui::IsItemHovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        const auto& t = world->GetComponent<TransformComponent>(selected[0]);
        Vector2 handleFbPos = renderSystem->WorldToFramebuffer({ t.worldX, t.worldY }, view);
        Vector2 mouseFbPos = sceneService.ScreenToFramebuffer(GetMousePosition());
        if (Vector2Distance(mouseFbPos, handleFbPos) <= Systems::RenderSystem::kMoveHandleRadius) {
            isDraggingGizmo_ = true;
            gizmoEntity_ = selected[0];
            lastGizmoFbPos_ = mouseFbPos;
            return;
        }
    }

    HandleViewportClick(sceneService, editorService, view);
}

void ViewportEditor::HandleViewportClick(SceneService& sceneService, EditorService& editorService, const CameraView& view) {
    if (!ImGui::IsItemClicked(ImGuiMouseButton_Left))
        return;

    auto* renderSystem = Systems::RenderSystem::GetCurrent();
    if (!renderSystem)
        return;

    Vector2 fbPos = sceneService.ScreenToFramebuffer(GetMousePosition());
    auto hits = renderSystem->Pick(fbPos, view);

    bool samePos = !hits.empty() && Vector2Distance(fbPos, lastClickFbPos_) < 4.0f;
    size_t index = samePos ? (lastClickIndex_ + 1) % hits.size() : 0;
    lastClickFbPos_ = fbPos;
    lastClickIndex_ = index;

    if (!hits.empty()) {
        editorService.SelectEntity(hits[index]);
    } else {
        editorService.ClearSelection();
    }
}

void ViewportEditor::ApplyGizmoDrag(World* world, Entity entity, float zoom, Vector2 fbDelta) {
    zoom = zoom != 0.0f ? zoom : 1.0f;
    Vector2 worldDelta = { fbDelta.x / zoom, fbDelta.y / zoom };

    // Matches TransformSystem::ComposeRecursive's local->world composition, inverted:
    // world = parentWorld.pos + rotate(local * parentWorld.scale, parentWorld.rotation).
    Vector2 localDelta = worldDelta;
    if (world->HasComponent<ParentComponent>(entity)) {
        Entity parent = world->GetComponent<ParentComponent>(entity).parent;
        if (parent != INVALID_ENTITY && world->HasComponent<TransformComponent>(parent)) {
            const auto& parentT = world->GetComponent<TransformComponent>(parent);
            float rad = -parentT.worldRotation * DEG2RAD;
            float cs = cosf(rad), sn = sinf(rad);
            Vector2 rotated = {
                worldDelta.x * cs - worldDelta.y * sn,
                worldDelta.x * sn + worldDelta.y * cs
            };
            float parentScaleX = parentT.worldScaleX != 0.0f ? parentT.worldScaleX : 1.0f;
            float parentScaleY = parentT.worldScaleY != 0.0f ? parentT.worldScaleY : 1.0f;
            localDelta = { rotated.x / parentScaleX, rotated.y / parentScaleY };
        }
    }

    auto& transform = world->GetComponent<TransformComponent>(entity);
    transform.localX += localDelta.x;
    transform.localY += localDelta.y;
}

}  // namespace Elysium

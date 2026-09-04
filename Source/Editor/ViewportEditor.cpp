#include "ViewportEditor.h"
#include <algorithm>
#include <cmath>
#include "Core/Application.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IEditorService.h"
#include "Interfaces/ISceneService.h"
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

ViewportEditor::ViewportEditor(ServiceLocator& services) : Editor(services, "Game") {}

void ViewportEditor::Draw() {
    Profile;

    auto& sceneService = services_.Get<ISceneService>();
    auto& editorService = services_.Get<IEditorService>();

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
        const auto& config = services_.Get<IApplicationService>().GetConfig();
        CameraView view{
            editorCam.position,
            editorCam.zoom != 0.0f ? editorCam.zoom : 1.0f,
            Rectangle{0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight}
        };

        // Where the framebuffer image landed on screen — rlImGuiImageRenderTextureFit centers
        // it maintaining aspect ratio. Needed by input handling and overlay drawing below, and
        // to tell SceneService where the game viewport is on screen.
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
        Rectangle imageScreenRect{drawX, drawY, drawW, drawH};

        HandleEditorCameraInput(sceneService, editorService);
        HandleGizmoOrPick(sceneService, editorService, view);
        DrawViewportOverlays(sceneService, editorService, view, imageScreenRect);

        sceneService.SetViewportRect(imageScreenRect);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportEditor::DrawToolbar(ISceneService& sceneService, IEditorService& editor) {
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

void ViewportEditor::InitializeEditorCameraIfNeeded(IEditorService& editorService) {
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

void ViewportEditor::HandleEditorCameraInput(ISceneService& sceneService, IEditorService& editorService) {
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
            const auto& config = services_.Get<IApplicationService>().GetConfig();
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

void ViewportEditor::HandleGizmoOrPick(ISceneService& sceneService, IEditorService& editorService, const CameraView& view) {
    auto* world = editorService.GetWorld();

    // Continue or end an in-progress drag. Either way, swallow this frame's click —
    // it belongs to the drag, not to re-picking.
    if (isDraggingGizmo_) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && world &&
            world->HasComponent<TransformComponent>(gizmoEntity_)) {
            Vector2 fbPos = sceneService.ScreenToFramebuffer(GetMousePosition());
            Vector2 fbDelta = { fbPos.x - lastGizmoFbPos_.x, fbPos.y - lastGizmoFbPos_.y };
            lastGizmoFbPos_ = fbPos;
            ApplyGizmoDrag(world, gizmoEntity_, view.zoom, fbDelta, gizmoIsWorldSpace_);
        } else {
            isDraggingGizmo_ = false;
        }
        return;
    }

    // Start a drag if this press landed on the single selected entity's move handle.
    const auto& selected = editorService.GetSelectedEntities();
    if (selected.size() == 1 && world &&
        world->HasComponent<TransformComponent>(selected[0]) &&
        ImGui::IsItemHovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        const auto& t = world->GetComponent<TransformComponent>(selected[0]);
        auto* renderSystem = sceneService.GetTopScene() ? sceneService.GetTopScene()->GetSystem<Systems::RenderSystem>() : nullptr;
        // An entity on a Screen2D layer already IS a framebuffer position — projecting it
        // through the camera view (as World2D entities need) would double-transform it.
        bool isWorldSpace = renderSystem ? renderSystem->GetEntityRenderInfo(selected[0]).isWorldSpace : true;
        Vector2 handleFbPos = isWorldSpace
            ? Systems::RenderProjector::WorldToFramebuffer({ t.worldX, t.worldY }, view)
            : Vector2{ t.worldX, t.worldY };
        Vector2 mouseFbPos = sceneService.ScreenToFramebuffer(GetMousePosition());
        if (Vector2Distance(mouseFbPos, handleFbPos) <= kMoveHandleRadius) {
            isDraggingGizmo_ = true;
            gizmoEntity_ = selected[0];
            lastGizmoFbPos_ = mouseFbPos;
            gizmoIsWorldSpace_ = isWorldSpace;
            return;
        }
    }

    HandleViewportClick(sceneService, editorService, view);
}

void ViewportEditor::HandleViewportClick(ISceneService& sceneService, IEditorService& editorService, const CameraView& view) {
    if (!ImGui::IsItemClicked(ImGuiMouseButton_Left))
        return;

    auto* renderSystem = sceneService.GetTopScene() ? sceneService.GetTopScene()->GetSystem<Systems::RenderSystem>() : nullptr;
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

Vector2 ViewportEditor::FramebufferToScreen(Vector2 fbPos, Rectangle imageScreenRect) const {
    const auto& config = services_.Get<IApplicationService>().GetConfig();
    return {
        imageScreenRect.x + (fbPos.x / (float)config.framebufferWidth)  * imageScreenRect.width,
        imageScreenRect.y + (fbPos.y / (float)config.framebufferHeight) * imageScreenRect.height
    };
}

void ViewportEditor::DrawViewportOverlays(ISceneService& sceneService, IEditorService& editorService,
                                           const CameraView& view, Rectangle imageScreenRect) {
    auto* world = editorService.GetWorld();
    if (!world) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(ImVec2(imageScreenRect.x, imageScreenRect.y),
                            ImVec2(imageScreenRect.x + imageScreenRect.width, imageScreenRect.y + imageScreenRect.height),
                            true);

    // isWorldSpace true: pos is a world coordinate, needs the camera view projection.
    // isWorldSpace false: pos is already a framebuffer position (a Screen2D-layer entity) —
    // projecting it through the camera view again would double-transform it.
    auto project = [&](Vector2 pos, bool isWorldSpace) {
        Vector2 fbPos = isWorldSpace ? Systems::RenderProjector::WorldToFramebuffer(pos, view) : pos;
        return FramebufferToScreen(fbPos, imageScreenRect);
    };

    // Origin axes — always a world-space concept.
    constexpr float kAxisExtent = 1'000'000.0f;
    Vector2 xStart = project({-kAxisExtent, 0.0f}, true);
    Vector2 xEnd   = project({ kAxisExtent, 0.0f}, true);
    Vector2 yStart = project({0.0f, -kAxisExtent}, true);
    Vector2 yEnd   = project({0.0f,  kAxisExtent}, true);
    drawList->AddLine(ImVec2(xStart.x, xStart.y), ImVec2(xEnd.x, xEnd.y), IM_COL32(255, 0, 0, 255), 1.5f);
    drawList->AddLine(ImVec2(yStart.x, yStart.y), ImVec2(yEnd.x, yEnd.y), IM_COL32(0, 255, 0, 255), 1.5f);

    // Each real CameraComponent's viewport bounds, since the editor doesn't render through them
    // directly — always a world-space concept too.
    const ImU32 cameraGizmoColor = IM_COL32(255, 60, 60, 255);
    world->Query<CameraComponent>([&](Entity entity, auto& camera) {
        if (!world->HasComponent<TransformComponent>(entity)) return;
        const auto& t = world->GetComponent<TransformComponent>(entity);
        float zoom = camera.zoom != 0.0f ? camera.zoom : 1.0f;
        float halfW = (camera.viewport.width  * 0.5f) / zoom;
        float halfH = (camera.viewport.height * 0.5f) / zoom;
        Vector2 tl = project({t.worldX - halfW, t.worldY - halfH}, true);
        Vector2 br = project({t.worldX + halfW, t.worldY + halfH}, true);
        drawList->AddRect(ImVec2(tl.x, tl.y), ImVec2(br.x, br.y), cameraGizmoColor, 0.0f, 0, 2.0f);
    });

    // Selection highlight, sized via the entity's RenderableType::Bounds where it has one
    // (falls back to a fixed box for renderable-less/currently-culled selected entities).
    auto* renderSystem = sceneService.GetTopScene() ? sceneService.GetTopScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    const auto& selected = editorService.GetSelectedEntities();
    constexpr float kDefaultHalfSize = 16.0f;
    for (Entity entity : selected) {
        Systems::EntityRenderInfo info = renderSystem ? renderSystem->GetEntityRenderInfo(entity) : Systems::EntityRenderInfo{};
        std::optional<Rectangle> bounds = info.bounds;
        if (!bounds && world->HasComponent<TransformComponent>(entity)) {
            const auto& t = world->GetComponent<TransformComponent>(entity);
            bounds = Rectangle{ t.worldX - kDefaultHalfSize, t.worldY - kDefaultHalfSize,
                                 kDefaultHalfSize * 2.0f, kDefaultHalfSize * 2.0f };
        }
        if (!bounds) continue;
        Vector2 tl = project({ bounds->x, bounds->y }, info.isWorldSpace);
        Vector2 br = project({ bounds->x + bounds->width, bounds->y + bounds->height }, info.isWorldSpace);
        drawList->AddRect(ImVec2(tl.x, tl.y), ImVec2(br.x, br.y), IM_COL32(255, 200, 0, 255), 0.0f, 0, 2.0f);
    }

    // Move handle: constant on-screen size regardless of zoom, matches kMoveHandleRadius's hit-test.
    if (selected.size() == 1 && world->HasComponent<TransformComponent>(selected[0])) {
        const auto& t = world->GetComponent<TransformComponent>(selected[0]);
        bool isWorldSpace = renderSystem ? renderSystem->GetEntityRenderInfo(selected[0]).isWorldSpace : true;
        Vector2 handlePos = project({t.worldX, t.worldY}, isWorldSpace);
        ImVec2 center(handlePos.x, handlePos.y);
        drawList->AddCircleFilled(center, kMoveHandleRadius, IM_COL32(0, 220, 255, 255));
        drawList->AddCircle(center, kMoveHandleRadius, IM_COL32(15, 15, 15, 255));
        float crossHalf = kMoveHandleRadius * 0.5f;
        drawList->AddLine(ImVec2(center.x - crossHalf, center.y), ImVec2(center.x + crossHalf, center.y), IM_COL32_WHITE, 1.5f);
        drawList->AddLine(ImVec2(center.x, center.y - crossHalf), ImVec2(center.x, center.y + crossHalf), IM_COL32_WHITE, 1.5f);
    }

    drawList->PopClipRect();
}

void ViewportEditor::ApplyGizmoDrag(World* world, Entity entity, float zoom, Vector2 fbDelta, bool isWorldSpace) {
    zoom = zoom != 0.0f ? zoom : 1.0f;
    // A Screen2D entity's position is already a framebuffer pixel — a 1px mouse move should
    // be a 1-unit local move, not scaled by the camera's zoom (which never touches it at render time).
    Vector2 worldDelta = isWorldSpace ? Vector2{ fbDelta.x / zoom, fbDelta.y / zoom } : fbDelta;

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

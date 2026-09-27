#include "ViewportEditor.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <ImGuizmo.h>
#include "Core/Application.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IEditorService.h"
#include "Interfaces/ISceneService.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Core/World.h"
#include "Core/PrefabInstance.h"
#include "Components/CameraComponent.h"
#include "Components/ParentComponent.h"
#include "Components/TransformComponent.h"
#include "Editor/Widgets.h"
#include "Editor/HierarchyEditor.h"
#include "Systems/RenderSystem.h"
#include "Core/Input.h"
#include "Core/MathTypes.h"

namespace Elysium {

using namespace Services;
using Systems::CameraView;

namespace {
// Ctrl-drag snapping, per gizmo mode.
constexpr float MoveSnap = 8.0f;     // world units
constexpr float RotateSnap = 15.0f;  // degrees
constexpr float ScaleSnap = 0.25f;

// Column-major 4x4s, the layout ImGuizmo reads.
struct Matrix4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

// The orthographic projection of what `view` shows. World y grows downward, so the top of
// the screen is the smaller y.
Matrix4 ViewProjection(const CameraView& view) {
    const float halfW = view.viewport.width * 0.5f / view.zoom;
    const float halfH = view.viewport.height * 0.5f / view.zoom;
    const float left = view.position.x - halfW, right = view.position.x + halfW;
    const float top = view.position.y - halfH, bottom = view.position.y + halfH;
    Matrix4 p;
    p.m[0] = 2.0f / (right - left);
    p.m[5] = 2.0f / (top - bottom);
    p.m[10] = -1.0f;
    p.m[12] = -(right + left) / (right - left);
    p.m[13] = -(top + bottom) / (top - bottom);
    return p;
}

// Same composition as TransformSystem: translate * rotate * scale.
Matrix4 EntityMatrix(Vector2 position, float rotationDegrees, float scaleX, float scaleY) {
    const float c = cosf(rotationDegrees * DegToRad), s = sinf(rotationDegrees * DegToRad);
    Matrix4 e;
    e.m[0] = c * scaleX;  e.m[1] = s * scaleX;
    e.m[4] = -s * scaleY; e.m[5] = c * scaleY;
    e.m[12] = position.x; e.m[13] = position.y;
    return e;
}

// Orders `entities` shallowest in the hierarchy first; ties go to whichever the Hierarchy
// panel lists first (roots in world order, children in sibling order, depth-first).
// Placed prefabs are black boxes: a hit on any of their entities is a hit on the instance root.
void ResolvePrefabRoots(const World& world, std::vector<Entity>& entities) {
    std::vector<Entity> resolved;
    for (Entity entity : entities) {
        const Entity root = PrefabInstances::RootOf(world, entity);
        if (std::find(resolved.begin(), resolved.end(), root) == resolved.end()) resolved.push_back(root);
    }
    entities = std::move(resolved);
}

void SortByHierarchy(const World& world, std::vector<Entity>& entities) {
    std::unordered_map<Entity, std::pair<int, int>> rank;  // entity -> (depth, display order)
    int order = 0;
    std::function<void(Entity, int)> visit = [&](Entity entity, int depth) {
        rank[entity] = {depth, order++};
        for (Entity child : world.GetChildren(entity)) visit(child, depth + 1);
    };
    for (Entity entity : world.GetLivingEntities()) {
        if (world.GetParent(entity) == INVALID_ENTITY) visit(entity, 0);
    }
    std::stable_sort(entities.begin(), entities.end(), [&](Entity a, Entity b) {
        auto ra = rank.find(a), rb = rank.find(b);
        if (ra == rank.end() || rb == rank.end()) return ra != rank.end();
        return ra->second < rb->second;
    });
}

float WrapDegrees(float degrees) {
    degrees = fmodf(degrees + 180.0f, 360.0f);
    return (degrees < 0.0f ? degrees + 360.0f : degrees) - 180.0f;
}

// Gizmo colors follow the theme: axes as in the overlays, the rotation ring in the accent.
void ApplyGizmoStyle(const EditorStyle::Palette& palette) {
    ImGuizmo::Style& style = ImGuizmo::GetStyle();
    style.Colors[ImGuizmo::DIRECTION_X] = palette.AxisX;
    style.Colors[ImGuizmo::DIRECTION_Y] = palette.AxisY;
    style.Colors[ImGuizmo::DIRECTION_Z] = palette.Accent;
    style.Colors[ImGuizmo::PLANE_Z] = EditorStyle::Palette::WithAlpha(palette.Accent, 0.38f);
    style.Colors[ImGuizmo::SELECTION] = palette.Selection;
}
}  // namespace

ViewportEditor::ViewportEditor(ServiceLocator& services) : Editor(services, Title) {}

void ViewportEditor::Draw() {
    Profile;

    auto& sceneService = services_.Get<ISceneService>();
    auto& editorService = services_.Get<IEditorService>();

    InitializeEditorCameraIfNeeded(editorService);

    // Opening a scene or switching document tabs: bring the Hierarchy forward (over Scenes)
    // so the newly shown world's entities are what's on screen.
    if (Scene* shown = editorService.GetViewportScene(); shown != lastViewportScene_) {
        if (shown) ImGui::SetWindowFocus(HierarchyEditor::Title);
        lastViewportScene_ = shown;
    }
    ImGuizmo::BeginFrame();

    if (BeginWindow(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        DrawDocumentTabs(sceneService, editorService);

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            editorService.SaveActiveDocument();
        }

        // Toolbar sits on a raised band spanning the window, bordered off from the image.
        const ImGuiStyle& style = ImGui::GetStyle();
        const float windowLeft = ImGui::GetWindowPos().x;
        const float windowRight = windowLeft + ImGui::GetWindowWidth();
        const float bandTop = ImGui::GetCursorScreenPos().y - style.ItemSpacing.y;
        const float bandBottom = ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight() + style.WindowPadding.y;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(ImVec2(windowLeft, bandTop), ImVec2(windowRight, bandBottom), Palette().ToU32(Palette().Mantle));
        drawList->AddLine(ImVec2(windowLeft, bandBottom), ImVec2(windowRight, bandBottom), Palette().ToU32(Palette().Border));

        DrawToolbar(sceneService, editorService);
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, bandBottom + style.ItemSpacing.y));

        // The image runs edge to edge under the padded toolbar.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("ViewportImage", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        // The scene renders at exactly this panel's size (from the next frame on, after a
        // resize), so the image is drawn 1:1 in the top-left.
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        sceneService.SetFramebufferSize((int)avail.x, (int)avail.y);

        const Framebuffer& fb = sceneService.GetFramebuffer();
        const Rectangle imageScreenRect{pos.x, pos.y, (float)fb.Width(), (float)fb.Height()};

        // V-flipped: framebuffer textures use GL bottom-up origin.
        ImGui::Image((ImTextureID)fb.TextureId(), ImVec2(imageScreenRect.width, imageScreenRect.height), ImVec2(0, 1), ImVec2(1, 0));
        if (!editorService.GetViewportScene()) {
            const char* hint = "Open a scene (Scenes panel) or a prefab (Assets panel)";
            const ImVec2 size = ImGui::CalcTextSize(hint);
            ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + (avail.x - size.x) * 0.5f, pos.y + (avail.y - size.y) * 0.5f),
                                                Palette().ToU32(Palette().TextMuted), hint);
        }
        const bool imageHovered = ImGui::IsItemHovered();
        const bool imageClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool imageRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        sceneService.SetViewportRect(imageScreenRect);

        // Snapshot the view that produced the currently-displayed framebuffer image
        // (i.e. before this frame's pan/zoom input is applied) so picking and the gizmo
        // line up with what's actually on screen right now.
        auto& editorCam = editorService.GetEditorCamera();
        CameraView view{
            editorCam.position,
            editorCam.zoom != 0.0f ? editorCam.zoom : 1.0f,
            Rectangle{0, 0, (float)fb.Width(), (float)fb.Height()}
        };
        if (auto* scene = editorService.GetViewportScene()) {
            if (auto* renderSystem = scene->GetSystem<Systems::RenderSystem>()) renderSystem->PlaceScreenInWorld(view);
        }

        DrawViewportOverlays(sceneService, editorService, view, imageScreenRect);
        const bool gizmoOwnsMouse = HandleGizmo(sceneService, editorService, view, imageScreenRect);
        // The gizmo gets the mouse first; an already-started pan keeps it until release.
        const bool canInteract = imageHovered && !gizmoOwnsMouse;
        if (canInteract || isPanningCamera_) HandleEditorCameraInput(sceneService, editorService, view, canInteract);
        if (canInteract) {
            HandleGizmoShortcuts(sceneService);
            if (imageClicked) HandleViewportClick(sceneService, editorService, view);
            if (imageRightClicked) OpenPickMenu(sceneService, editorService, view);
        }
        DrawPickMenu(editorService);

        ImGui::EndChild();
    }
    EndWindow();
}

// One closeable tab per open document (scene or prefab), each its own copy loaded from disk.
void ViewportEditor::DrawDocumentTabs(ISceneService& sceneService, IEditorService& editor) {
    const auto& documents = editor.GetDocuments();
    const int active = editor.GetActiveDocument();
    // The service changed the active document (e.g. a prefab was just opened): make ImGui follow.
    const bool followService = active != shownDocument_;

    int selected = active;
    int closeIndex = -1;
    if (ImGui::BeginTabBar("Documents", ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (int i = 0; i < (int)documents.size(); ++i) {
            bool open = true;
            const char* icon = documents[i]->IsPrefab() ? ICON_FA_BOX "  " : ICON_FA_CUBES "  ";
            std::string label = icon + documents[i]->title + "###" + documents[i]->fullPath;
            if (ImGui::BeginTabItem(label.c_str(), &open, followService && active == i ? ImGuiTabItemFlags_SetSelected : 0)) {
                selected = i;
                ImGui::EndTabItem();
            }
            ItemTooltip(documents[i]->fullPath.c_str());
            if (!open) closeIndex = i;
        }
        ImGui::EndTabBar();
    }

    if (!followService && selected != active) editor.SetActiveDocument(selected);
    if (closeIndex >= 0) editor.CloseDocument(closeIndex);
    shownDocument_ = editor.GetActiveDocument();
}

void ViewportEditor::DrawToolbar(ISceneService& sceneService, IEditorService& editor) {
    // No play/pause: the editor never simulates its documents, so nothing a simulation
    // moved can be saved by mistake. Play mode (F2) runs the saved files instead.
    ImGui::BeginDisabled(editor.GetActiveDocument() < 0);
    if (IconButton(ICON_FA_FLOPPY_DISK, "Save (Ctrl+S)")) editor.SaveActiveDocument();
    ImGui::EndDisabled();

    // Gizmo mode.
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
    if (ToggleIconButton(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, gizmoMode_ == GizmoMode::Move, "Move (W)")) gizmoMode_ = GizmoMode::Move;
    ImGui::SameLine();
    if (ToggleIconButton(ICON_FA_ROTATE, gizmoMode_ == GizmoMode::Rotate, "Rotate (E)")) gizmoMode_ = GizmoMode::Rotate;
    ImGui::SameLine();
    if (ToggleIconButton(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, gizmoMode_ == GizmoMode::Scale, "Scale (R)")) gizmoMode_ = GizmoMode::Scale;

    // Right side: editor camera readout and a reset back to the scene's camera.
    auto& camera = editor.GetEditorCamera();
    char readout[64];
    snprintf(readout, sizeof(readout), "%.0f, %.0f   %.0f%%", camera.position.x, camera.position.y, camera.zoom * 100.0f);
    AlignRight(ImGui::CalcTextSize(readout).x + ButtonWidth(ICON_FA_CROSSHAIRS) + ImGui::GetStyle().ItemSpacing.x);
    ColoredText(Palette().TextMuted, readout);
    ImGui::SameLine();
    if (IconButton(ICON_FA_CROSSHAIRS, "Reset view to the scene camera")) camera.initialized = false;
}

void ViewportEditor::HandleGizmoShortcuts(ISceneService& sceneService) {
    // While simulating, these keys belong to the game.
    if (ImGui::GetIO().WantTextInput || ImGui::GetIO().KeyCtrl) return;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoMode_ = GizmoMode::Move;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoMode_ = GizmoMode::Rotate;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) gizmoMode_ = GizmoMode::Scale;
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

void ViewportEditor::HandleEditorCameraInput(ISceneService& sceneService, IEditorService& editorService,
                                             const CameraView& view, bool hovered) {
    auto& cam = editorService.GetEditorCamera();

    if (hovered && Input::IsMouseButtonPressed(MouseButton::Middle)) {
        isPanningCamera_ = true;
    }
    if (!Input::IsMouseButtonDown(MouseButton::Middle)) {
        isPanningCamera_ = false;
    }

    if (isPanningCamera_) {
        Vector2 delta = Input::GetMouseDelta();
        float zoom = cam.zoom != 0.0f ? cam.zoom : 1.0f;
        cam.position.x -= delta.x / zoom;
        cam.position.y -= delta.y / zoom;
        return;
    }

    float wheel = hovered ? Input::GetMouseWheelMove() : 0.0f;
    if (wheel != 0.0f) {
        // Find the world point under the cursor before changing zoom, then re-solve the
        // camera position so that same world point stays under the cursor.
        Vector2 mouseFbPos = sceneService.ScreenToFramebuffer(Input::GetMousePosition());
        Vector2 worldUnderMouse = Systems::RenderProjector::FramebufferToWorld(mouseFbPos, view);

        float factor = 1.0f + wheel * 0.1f;
        float newZoom = std::clamp(cam.zoom * factor, 0.1f, 10.0f);
        cam.zoom = newZoom;

        Vector2 viewportCenter = { view.viewport.width * 0.5f, view.viewport.height * 0.5f };
        cam.position.x = worldUnderMouse.x - (mouseFbPos.x - viewportCenter.x) / newZoom;
        cam.position.y = worldUnderMouse.y - (mouseFbPos.y - viewportCenter.y) / newZoom;
    }
}

bool ViewportEditor::HandleGizmo(ISceneService& sceneService, IEditorService& editorService,
                                 const CameraView& view, Rectangle imageScreenRect) {
    auto* world = editorService.GetWorld();
    const auto& selected = editorService.GetSelectedEntities();
    if (!world || selected.size() != 1 || !world->HasComponent<TransformComponent>(selected[0])) return false;
    const Entity entity = selected[0];

    // A Screen2D entity's position is a game-screen pixel, which the editor shows in the
    // world at view.screenOrigin + p * screenScale; the gizmo works in that world position.
    auto* renderSystem = editorService.GetViewportScene() ? editorService.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    const bool isWorldSpace = renderSystem ? renderSystem->GetEntityRenderInfo(entity).isWorldSpace : true;
    const float screenScale = view.screenScale != 0.0f ? view.screenScale : 1.0f;
    auto toShown = [&](Vector2 p) {
        return isWorldSpace ? p : Vector2{ view.screenOrigin.x + p.x * screenScale, view.screenOrigin.y + p.y * screenScale };
    };
    auto fromShown = [&](Vector2 p) {
        return isWorldSpace ? p : Vector2{ (p.x - view.screenOrigin.x) / screenScale, (p.y - view.screenOrigin.y) / screenScale };
    };

    auto& t = world->GetComponent<TransformComponent>(entity);
    Matrix4 matrix = EntityMatrix(toShown({ t.worldX, t.worldY }), t.worldRotation, t.worldScaleX, t.worldScaleY);
    const Matrix4 identity;
    const Matrix4 projection = ViewProjection(view);

    ApplyGizmoStyle(Palette());
    ImGuizmo::SetOrthographic(true);
    ImGuizmo::AllowAxisFlip(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(imageScreenRect.x, imageScreenRect.y, imageScreenRect.width, imageScreenRect.height);

    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y);
    ImGuizmo::MODE mode = ImGuizmo::WORLD;
    float snapValue = MoveSnap;
    if (gizmoMode_ == GizmoMode::Rotate) {
        operation = ImGuizmo::ROTATE_Z;
        snapValue = RotateSnap;
    } else if (gizmoMode_ == GizmoMode::Scale) {
        operation = ImGuizmo::OPERATION(ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y);
        mode = ImGuizmo::LOCAL;
        snapValue = ScaleSnap;
    }
    const float snap[3] = { snapValue, snapValue, snapValue };
    const bool snapping = ImGui::GetIO().KeyCtrl;

    if (ImGuizmo::Manipulate(identity.m, projection.m, operation, mode, matrix.m, nullptr, snapping ? snap : nullptr)) {
        const float* m = matrix.m;
        // Each mode writes back only what it edits, so a move never rewrites (and rounds)
        // the rotation or scale. Changes land in the local transform; TransformSystem
        // recomposes world next frame.
        if (gizmoMode_ == GizmoMode::Move) {
            Vector2 target = fromShown({ m[12], m[13] });
            Vector2 local = target;
            if (world->HasComponent<ParentComponent>(entity)) {
                Entity parent = world->GetComponent<ParentComponent>(entity).parent;
                if (parent != INVALID_ENTITY && world->HasComponent<TransformComponent>(parent)) {
                    // TransformSystem::ComposeRecursive inverted:
                    // world = parent.pos + rotate(local * parent.scale, parent.rotation).
                    const auto& p = world->GetComponent<TransformComponent>(parent);
                    const float rad = -p.worldRotation * DegToRad;
                    const float c = cosf(rad), s = sinf(rad);
                    const Vector2 d = { target.x - p.worldX, target.y - p.worldY };
                    const Vector2 rotated = { d.x * c - d.y * s, d.x * s + d.y * c };
                    local = { rotated.x / (p.worldScaleX != 0.0f ? p.worldScaleX : 1.0f),
                              rotated.y / (p.worldScaleY != 0.0f ? p.worldScaleY : 1.0f) };
                }
            }
            t.localX = local.x;
            t.localY = local.y;
        } else if (gizmoMode_ == GizmoMode::Rotate) {
            // World rotation is parent + local, so the world delta is the local delta.
            const float newRotation = atan2f(m[1], m[0]) / DegToRad;
            t.localRotation += WrapDegrees(newRotation - t.worldRotation);
        } else {
            // World scale is parent * local, so the ratio carries over. Ratios of the axis
            // lengths keep a flipped (negative) scale flipped.
            const float oldX = std::fabs(t.worldScaleX), oldY = std::fabs(t.worldScaleY);
            if (oldX > 0.0f) t.localScaleX *= std::hypot(m[0], m[1]) / oldX;
            if (oldY > 0.0f) t.localScaleY *= std::hypot(m[4], m[5]) / oldY;
        }
    }
    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

void ViewportEditor::HandleViewportClick(ISceneService& sceneService, IEditorService& editorService, const CameraView& view) {
    auto* renderSystem = editorService.GetViewportScene() ? editorService.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    if (!renderSystem)
        return;

    Vector2 fbPos = sceneService.ScreenToFramebuffer(Input::GetMousePosition());
    auto hits = renderSystem->Pick(fbPos, view);
    if (auto* world = editorService.GetWorld()) {
        ResolvePrefabRoots(*world, hits);
        SortByHierarchy(*world, hits);
    }

    bool samePos = !hits.empty() && (fbPos - lastClickFbPos_).Length() < Theme().ClickCycleDistance;
    size_t index = samePos ? (lastClickIndex_ + 1) % hits.size() : 0;
    lastClickFbPos_ = fbPos;
    lastClickIndex_ = index;

    if (!hits.empty()) {
        editorService.SelectEntity(hits[index]);
    } else {
        editorService.ClearSelection();
    }
}

// Right-click: everything under the cursor, topmost first, to pick from when entities overlap.
void ViewportEditor::OpenPickMenu(ISceneService& sceneService, IEditorService& editorService, const CameraView& view) {
    auto* renderSystem = editorService.GetViewportScene() ? editorService.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    if (!renderSystem) return;
    pickMenuHits_ = renderSystem->Pick(sceneService.ScreenToFramebuffer(Input::GetMousePosition()), view);
    if (auto* world = editorService.GetWorld()) ResolvePrefabRoots(*world, pickMenuHits_);
    ImGui::OpenPopup("ViewportPick");
}

void ViewportEditor::DrawPickMenu(IEditorService& editorService) {
    if (!ImGui::BeginPopup("ViewportPick")) return;
    auto* world = editorService.GetWorld();
    SectionHeader("Under Cursor");
    if (!world || pickMenuHits_.empty()) MutedText("Nothing here");
    for (Entity entity : pickMenuHits_) {
        if (!world || !world->IsAlive(entity)) continue;  // destroyed since the pick
        ImGui::PushID((int)entity);
        const std::string label = EntityLabel(world->GetEntityName(entity), entity);
        if (ImGui::Selectable(label.c_str(), editorService.IsSelected(entity))) {
            editorService.SelectEntity(entity, ImGui::GetIO().KeyShift);
        }
        ImGui::PopID();
    }
    ImGui::EndPopup();
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
    // isWorldSpace false: pos is a screen pixel (a Screen2D-layer entity), placed in the
    // world where the game camera shows it (RenderSystem::PlaceScreenInWorld). The image is
    // drawn 1:1, so a framebuffer pixel is just offset to where the image sits.
    auto project = [&](Vector2 pos, bool isWorldSpace) {
        Vector2 fbPos = isWorldSpace ? Systems::RenderProjector::WorldToFramebuffer(pos, view)
                                     : Systems::RenderProjector::ScreenToFramebuffer(pos, view);
        return Vector2{ imageScreenRect.x + fbPos.x, imageScreenRect.y + fbPos.y };
    };

    // Origin axes — always a world-space concept.
    constexpr float kAxisExtent = 1'000'000.0f;
    Vector2 xStart = project({-kAxisExtent, 0.0f}, true);
    Vector2 xEnd   = project({ kAxisExtent, 0.0f}, true);
    Vector2 yStart = project({0.0f, -kAxisExtent}, true);
    Vector2 yEnd   = project({0.0f,  kAxisExtent}, true);
    drawList->AddLine(ImVec2(xStart.x, xStart.y), ImVec2(xEnd.x, xEnd.y), Palette().ToU32(Palette().AxisX), Theme().AxisWidth);
    drawList->AddLine(ImVec2(yStart.x, yStart.y), ImVec2(yEnd.x, yEnd.y), Palette().ToU32(Palette().AxisY), Theme().AxisWidth);

    // Each real CameraComponent's viewport bounds, since the editor doesn't render through them
    // directly — always a world-space concept too.
    const ImU32 cameraGizmoColor = Palette().ToU32(Palette().CameraBounds);
    world->Query<CameraComponent>([&](Entity entity, auto& camera) {
        if (!world->HasComponent<TransformComponent>(entity)) return;
        const auto& t = world->GetComponent<TransformComponent>(entity);
        float zoom = camera.zoom != 0.0f ? camera.zoom : 1.0f;
        float halfW = (camera.viewport.width  * 0.5f) / zoom;
        float halfH = (camera.viewport.height * 0.5f) / zoom;
        Vector2 tl = project({t.worldX - halfW, t.worldY - halfH}, true);
        Vector2 br = project({t.worldX + halfW, t.worldY + halfH}, true);
        drawList->AddRect(ImVec2(tl.x, tl.y), ImVec2(br.x, br.y), cameraGizmoColor, 0.0f, 0, Theme().OverlayLineWidth);
    });

    // Selection highlight, sized via the entity's RenderableType::Bounds where it has one
    // (falls back to a fixed box for renderable-less/currently-culled selected entities).
    auto* renderSystem = editorService.GetViewportScene() ? editorService.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    const auto& selected = editorService.GetSelectedEntities();
    const ImU32 selectionColor = Palette().ToU32(Palette().Selection);
    for (Entity entity : selected) {
        Systems::EntityRenderInfo info = renderSystem ? renderSystem->GetEntityRenderInfo(entity) : Systems::EntityRenderInfo{};
        std::optional<Rectangle> bounds = info.bounds;
        if (!bounds && world->HasComponent<TransformComponent>(entity)) {
            const auto& t = world->GetComponent<TransformComponent>(entity);
            bounds = Rectangle{ t.worldX - Theme().SelectionFallback, t.worldY - Theme().SelectionFallback,
                                 Theme().SelectionFallback * 2.0f, Theme().SelectionFallback * 2.0f };
        }
        if (!bounds) continue;
        Vector2 tl = project({ bounds->x, bounds->y }, info.isWorldSpace);
        Vector2 br = project({ bounds->x + bounds->width, bounds->y + bounds->height }, info.isWorldSpace);
        drawList->AddRect(ImVec2(tl.x, tl.y), ImVec2(br.x, br.y), selectionColor, 0.0f, 0, Theme().OverlayLineWidth);
    }

    drawList->PopClipRect();
}

}  // namespace Elysium

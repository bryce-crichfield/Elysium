#include "ViewportEditor.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
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
#include "Editor/OverlayPainter.h"
#include "Systems/NavMeshSystem.h"
#include "Core/Path.h"
#include "Editor/AssetField.h"
#include "Editor/AssetStyle.h"
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

// What a click under the cursor can select, most likely intent first.
// - A placed prefab is a black box, picked as its root. Its internal entities only stand in
//   for it when the root draws nothing itself, so a big child (a light's glow) doesn't make
//   the whole area around the prefab grab clicks.
// - Smallest drawn area first: the thing you aimed at, not the backdrop it sits on. Ties
//   keep paint order (topmost first).
std::vector<Entity> PickTargets(const World& world, Systems::RenderSystem& renderSystem, const std::vector<Entity>& hits) {
    auto area = [&](Entity entity) {
        const auto bounds = renderSystem.GetEntityRenderInfo(entity).bounds;
        return bounds ? std::fabs(bounds->width * bounds->height) : FLT_MAX;
    };
    std::vector<Entity> targets;
    for (Entity entity : hits) {
        const Entity root = PrefabInstances::RootOf(world, entity);
        if (root != entity && renderSystem.GetEntityRenderInfo(root).bounds) continue;
        if (std::find(targets.begin(), targets.end(), root) == targets.end()) targets.push_back(root);
    }
    std::stable_sort(targets.begin(), targets.end(), [&](Entity a, Entity b) { return area(a) < area(b); });
    return targets;
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

    // No title tab: the document tabs are the header.
    if (BeginWindow(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, false)) {
        DrawDocumentTabs(sceneService, editorService);
        const EditorDocument* document = editorService.GetActiveDocumentInfo();
        ContentPane* pane = document && !document->HasWorld() ? PaneFor(editorService, *document) : nullptr;

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            Save(editorService, pane);
        }

        // Toolbar sits on a raised band spanning the window, bordered off from the content.
        const ImGuiStyle& style = ImGui::GetStyle();
        const float windowLeft = ImGui::GetWindowPos().x;
        const float windowRight = windowLeft + ImGui::GetWindowWidth();
        const float bandTop = ImGui::GetCursorScreenPos().y - style.ItemSpacing.y;
        const float bandBottom = ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight() + style.WindowPadding.y;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(ImVec2(windowLeft, bandTop), ImVec2(windowRight, bandBottom), Palette().ToU32(Palette().Mantle));
        drawList->AddLine(ImVec2(windowLeft, bandBottom), ImVec2(windowRight, bandBottom), Palette().ToU32(Palette().Border));

        DrawToolbar(editorService, document, pane);
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, bandBottom + style.ItemSpacing.y));

        if (pane) pane->Draw();
        else if (document && document->HasWorld() && settingsOpen_.count(document->fullPath)) {
            // Settings edit the document in place.
            auto& doc = const_cast<EditorDocument&>(*document);
            if (doc.IsScene()) sceneSettings_.Draw(*doc.scene);
            else prefabSettings_.Draw(doc);
        } else DrawWorld(sceneService, editorService);
    }
    EndWindow();

    // Ctrl+S is the Viewport's own (above); these apply wherever focus is.
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S)) BeginSaveAs();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) BeginNew();
    HandleFileDialog(editorService);
}

void ViewportEditor::SaveActive() {
    auto& editor = services_.Get<IEditorService>();
    const EditorDocument* document = editor.GetActiveDocumentInfo();
    if (document) Save(editor, document->HasWorld() ? nullptr : PaneFor(editor, *document));
}

void ViewportEditor::BeginSaveAs() {
    if (const EditorDocument* document = services_.Get<IEditorService>().GetActiveDocumentInfo()) {
        fileDialog_.OpenSaveAs(document->kind, document->fullPath);
    }
}

void ViewportEditor::BeginNew() { fileDialog_.OpenNew(); }

void ViewportEditor::HandleFileDialog(IEditorService& editor) {
    const auto result = fileDialog_.Draw();
    if (!result) return;
    if (result->mode == AssetFileDialog::Mode::New) {
        editor.CreateAsset(result->kind, result->fullPath);
        return;
    }
    // Save As: scenes and prefabs are written by the service, other kinds by their pane.
    const EditorDocument* document = editor.GetActiveDocumentInfo();
    if (!document) return;
    if (document->HasWorld()) {
        editor.SaveActiveDocumentAs(result->fullPath);
    } else if (ContentPane* pane = PaneFor(editor, *document); pane && pane->SaveAs(result->fullPath)) {
        editor.ReplaceActiveDocument(result->fullPath);
    }
}

void ViewportEditor::DrawWorld(ISceneService& sceneService, IEditorService& editorService) {
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
    std::optional<std::string> droppedPrefab;
    if (editorService.GetWorld() && ImGui::BeginDragDropTarget()) {
        droppedPrefab = AcceptAssetDrop(AssetKind::Prefab);
        ImGui::EndDragDropTarget();
    }
    if (!editorService.GetViewportScene()) {
        const char* hint = "Open a scene or a prefab from the Assets panel";
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
        if (auto* renderSystem = scene->GetSystem<Systems::RenderSystem>()) {
            renderSystem->PlaceScreenInWorld(view);
            // The layer drawer's hide/solo, re-pushed every frame. The sorter applies it to its
            // own copy of the layer list, so the scene's SceneLayer flags stay untouched.
            renderSystem->SetHiddenLayerOverride(editorService.GetHiddenLayers());
        }
    }

    // A prefab dropped from the Assets panel is placed where it was dropped.
    if (droppedPrefab) PlaceDroppedPrefab(sceneService, editorService, view, *droppedPrefab);

    DrawViewportOverlays(sceneService, editorService, view, imageScreenRect);

    const ViewportInput input{
        .mouseWorld = Systems::RenderProjector::FramebufferToWorld(sceneService.ScreenToFramebuffer(Input::GetMousePosition()), view),
        .worldPerPixel = 1.0f / view.zoom,
        .hovered = imageHovered,
        .clicked = imageClicked,
        .rightClicked = imageRightClicked,
    };

    // Tool precedence: paint mode first (its whole job is the left button), then navmesh mode,
    // then the gizmo, then plain picking and the camera.
    bool toolConsumedClick = false;
    if (layerDrawer_.PaintMode() && editorService.GetWorld()) {
        toolConsumedClick = prefabPainter_.HandleInput(*editorService.GetWorld(), editorService, input,
                                                      layerDrawer_.BrushPrefab());
    } else {
        prefabPainter_.EndStroke();
    }
    if (!toolConsumedClick && navMeshTool_.IsActive() && editorService.GetWorld()) {
        toolConsumedClick = navMeshTool_.HandleInput(*editorService.GetWorld(), editorService, input);
    }
    const bool gizmoOwnsMouse = !layerDrawer_.PaintMode() && HandleGizmo(sceneService, editorService, view, imageScreenRect);
    // The gizmo gets the mouse first; an already-started pan keeps it until release.
    const bool canInteract = imageHovered && !gizmoOwnsMouse && !toolConsumedClick;
    if (canInteract || isPanningCamera_) HandleEditorCameraInput(sceneService, editorService, view, canInteract);
    if (canInteract) {
        HandleGizmoShortcuts(sceneService);
        if (imageClicked) HandleViewportClick(sceneService, editorService, view);
        if (imageRightClicked && !navMeshTool_.IsActive() && !layerDrawer_.PaintMode()) {
            OpenPickMenu(sceneService, editorService, view);
        }
    }
    DrawPickMenu(editorService);

    // Last, so it sits over the scene and its own clicks aren't treated as viewport clicks.
    if (auto* scene = editorService.GetViewportScene()) {
        const auto* document = editorService.GetActiveDocumentInfo();
        if (document && document->IsScene()) layerDrawer_.Draw(*scene, editorService, imageScreenRect);
    }

    ImGui::EndChild();
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
            // Tabs wear their asset kind's color and icon, as in the Assets panel.
            const AssetStyle kind = StyleOf(documents[i]->kind);
            const ImVec4 color = kind.color;
            std::string label = std::string(kind.icon) + "  " + documents[i]->title + "###" + documents[i]->fullPath;
            ImGui::PushStyleColor(ImGuiCol_Tab, Palette().WithAlpha(color, 0.22f));
            ImGui::PushStyleColor(ImGuiCol_TabHovered, Palette().WithAlpha(color, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_TabSelected, Palette().WithAlpha(color, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline, color);
            ImGui::PushStyleColor(ImGuiCol_TabDimmed, Palette().WithAlpha(color, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected, Palette().WithAlpha(color, 0.30f));
            const bool tabOpen = ImGui::BeginTabItem(label.c_str(), &open, followService && active == i ? ImGuiTabItemFlags_SetSelected : 0);
            ImGui::PopStyleColor(6);
            if (tabOpen) {
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

void ViewportEditor::DrawToolbar(IEditorService& editor, const EditorDocument* document, ContentPane* pane) {
    // No play/pause: the editor never simulates its documents, so nothing a simulation
    // moved can be saved by mistake. Play mode (F2) runs the saved files instead.
    const bool canSave = document && (document->HasWorld() || (pane && pane->CanSave()));
    ImGui::BeginDisabled(!canSave);
    if (IconButton(ICON_FA_FLOPPY_DISK, "Save (Ctrl+S)")) Save(editor, pane);
    ImGui::EndDisabled();

    if (pane) {
        pane->DrawToolbar();
        return;
    }

    // A scene or prefab can swap its world for its settings (a scene's layers and systems,
    // a prefab's parameters) and back.
    const bool settings = document && document->HasWorld() && settingsOpen_.count(document->fullPath);
    if (document && document->HasWorld()) {
        ImGui::SameLine();
        const std::string back = std::string(ICON_FA_ARROW_LEFT "  Back to ") + StyleOf(document->kind).label;
        if (settings ? ImGui::Button(back.c_str()) : ImGui::Button(ICON_FA_SLIDERS "  Settings")) {
            if (settings) settingsOpen_.erase(document->fullPath);
            else settingsOpen_.insert(document->fullPath);
        }
        ItemTooltip(settings ? "Back to the rendered view"
                             : document->IsScene() ? "The scene's layers and systems" : "The prefab's parameters");
    }
    if (settings) return;

    // Gizmo mode.
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
    if (ToggleIconButton(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, gizmoMode_ == GizmoMode::Move, "Move (W)")) gizmoMode_ = GizmoMode::Move;
    ImGui::SameLine();
    if (ToggleIconButton(ICON_FA_ROTATE, gizmoMode_ == GizmoMode::Rotate, "Rotate (E)")) gizmoMode_ = GizmoMode::Rotate;
    ImGui::SameLine();
    if (ToggleIconButton(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, gizmoMode_ == GizmoMode::Scale, "Scale (R)")) gizmoMode_ = GizmoMode::Scale;

    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
    DrawOverlaysMenu(overlays_);

    // Snap is the one grid control that belongs on the toolbar: it's toggled constantly while
    // working, unlike the spacing, which is set once on the Scene Settings screen.
    GridSettings& grid = editor.GetGrid();
    ImGui::SameLine();
    if (ToggleIconButton(ICON_FA_BORDER_ALL, grid.snapEnabled, "Snap to grid")) grid.snapEnabled = !grid.snapEnabled;
    ImGui::SameLine();
    if (ToggleIconButton(ICON_FA_TABLE_CELLS_LARGE, grid.showGrid, "Show grid")) grid.showGrid = !grid.showGrid;

    const bool isScene = document && document->IsScene();
    if (!isScene) {
        if (navMeshTool_.IsActive()) navMeshTool_.SetActive(false);
        // The drawer and the paint tool are scene-only; a prefab tab has no layer list.
        layerDrawer_.SetPaintMode(false);
        layerDrawer_.SetOpen(false);
    }
    if (isScene) {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
        layerDrawer_.DrawToolbarButton();
        ImGui::SameLine();
        if (ToggleIconButton(ICON_FA_ROUTE, navMeshTool_.IsActive(), "Navmesh mode")) navMeshTool_.SetActive(!navMeshTool_.IsActive());
        if (navMeshTool_.IsActive()) navMeshTool_.DrawToolbar();
        // Painting and navmesh editing both want the left button; entering one leaves the other.
        if (navMeshTool_.IsActive()) layerDrawer_.SetPaintMode(false);

        const std::string& activeLayer = editor.GetActiveLayer();
        if (!activeLayer.empty()) {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
            ColoredText(Palette().Accent, (std::string(ICON_FA_LAYER_GROUP) + "  " + activeLayer).c_str());
            ItemTooltip("Focused layer � the Hierarchy is filtered to it");
        }
    }

    // Right side: editor camera readout and a reset back to the scene's camera.
    auto& camera = editor.GetEditorCamera();
    char readout[64];
    snprintf(readout, sizeof(readout), "%.0f, %.0f   %.0f%%", camera.position.x, camera.position.y, camera.zoom * 100.0f);
    AlignRight(ImGui::CalcTextSize(readout).x + ButtonWidth(ICON_FA_CROSSHAIRS) + ImGui::GetStyle().ItemSpacing.x);
    ColoredText(Palette().TextMuted, readout);
    ImGui::SameLine();
    if (IconButton(ICON_FA_CROSSHAIRS, "Reset view to the scene camera")) camera.initialized = false;
}

void ViewportEditor::Save(IEditorService& editor, ContentPane* pane) {
    if (pane) pane->Save();
    else editor.SaveActiveDocument();
}

ContentPane* ViewportEditor::PaneFor(IEditorService& editor, const EditorDocument& document) {
    // Forget panes (and settings toggles) of tabs that have closed.
    auto isOpen = [&](const std::string& fullPath) {
        for (const auto& doc : editor.GetDocuments()) {
            if (doc->fullPath == fullPath) return true;
        }
        return false;
    };
    std::erase_if(panes_, [&](const auto& entry) { return !isOpen(entry.first); });
    std::erase_if(settingsOpen_, [&](const std::string& fullPath) { return !isOpen(fullPath); });

    auto& pane = panes_[document.fullPath];
    if (!pane) pane = MakeContentPane(services_, document);
    return pane.get();
}

std::unique_ptr<ContentPane> MakeContentPane(ServiceLocator& services, const EditorDocument& document) {
    switch (document.kind) {
        case AssetKind::Script:
        case AssetKind::Shader: return MakeCodePane(services, document);
        case AssetKind::Texture: return MakeTexturePane(services, document);
        case AssetKind::Sprite: return MakeSpritePane(services, document);
        default: return nullptr;
    }
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
    // Locked layer: no gizmo at all, so there's nothing to drag it by.
    if (editorService.IsEntityLocked(entity)) return false;

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
    // Ctrl is the ad-hoc snap. A move is instead snapped to the editing grid below, since
    // ImGuizmo's snap is axis-aligned and an isometric lattice isn't.
    const bool gridSnapsMove = gizmoMode_ == GizmoMode::Move && isWorldSpace && editorService.GetGrid().snapEnabled;
    const bool snapping = ImGui::GetIO().KeyCtrl && !gridSnapsMove;

    if (ImGuizmo::Manipulate(identity.m, projection.m, operation, mode, matrix.m, nullptr, snapping ? snap : nullptr)) {
        const float* m = matrix.m;
        // Each mode writes back only what it edits, so a move never rewrites (and rounds)
        // the rotation or scale. Changes land in the local transform; TransformSystem
        // recomposes world next frame.
        if (gizmoMode_ == GizmoMode::Move) {
            Vector2 target = fromShown({ m[12], m[13] });
            if (gridSnapsMove) target = editorService.SnapToGrid(target);
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

void ViewportEditor::PlaceDroppedPrefab(ISceneService& sceneService, IEditorService& editorService, const CameraView& view,
                                        const std::string& relativePath) {
    const Entity placed = editorService.InstantiatePrefab(Path(relativePath).GetFullPath());
    World* world = editorService.GetWorld();
    if (placed == INVALID_ENTITY || !world || !world->HasComponent<TransformComponent>(placed)) return;
    Vector2 target = editorService.SnapToGrid(
        Systems::RenderProjector::FramebufferToWorld(sceneService.ScreenToFramebuffer(Input::GetMousePosition()), view));
    // Local to its parent (a prefab document places under its root).
    const Entity parent = world->GetParent(placed);
    if (parent != INVALID_ENTITY && world->HasComponent<TransformComponent>(parent)) {
        const auto& parentTransform = world->GetComponent<TransformComponent>(parent);
        target.x -= parentTransform.worldX;
        target.y -= parentTransform.worldY;
    }
    auto& transform = world->GetComponent<TransformComponent>(placed);
    transform.localX = target.x;
    transform.localY = target.y;
}

// The grid is drawn from the visible world rect out, never from the lattice's extent: zoomed
// out far enough, an unbounded grid is both unreadable and enough vertices to blow ImGui's
// 16-bit draw-list indices. kMaxGridLines caps it and the grid simply fades out instead.
void ViewportEditor::DrawGrid(IEditorService& editorService, const CameraView& view,
                              Rectangle imageScreenRect, OverlayPainter& painter) {
    constexpr int kMaxGridLines = 400;
    const GridSettings& grid = editorService.GetGrid();
    if (!grid.showGrid) return;
    const Vector2 cell = grid.Cell();
    if (cell.x <= 0.0f || cell.y <= 0.0f) return;

    // The world rect the viewport currently shows.
    const Vector2 topLeft = Systems::RenderProjector::FramebufferToWorld({0.0f, 0.0f}, view);
    const Vector2 bottomRight = Systems::RenderProjector::FramebufferToWorld({view.viewport.width, view.viewport.height}, view);
    const float minX = std::min(topLeft.x, bottomRight.x), maxX = std::max(topLeft.x, bottomRight.x);
    const float minY = std::min(topLeft.y, bottomRight.y), maxY = std::max(topLeft.y, bottomRight.y);

    const ImVec4 color = Palette().WithAlpha(Palette().Border, 0.55f);

    if (grid.lattice == GridLattice::Square) {
        if ((maxX - minX) / cell.x > kMaxGridLines || (maxY - minY) / cell.y > kMaxGridLines) return;
        for (float x = std::floor(minX / cell.x) * cell.x; x <= maxX; x += cell.x) {
            painter.Line({x, minY}, {x, maxY}, color, 1.0f);
        }
        for (float y = std::floor(minY / cell.y) * cell.y; y <= maxY; y += cell.y) {
            painter.Line({minX, y}, {maxX, y}, color, 1.0f);
        }
        return;
    }

    // Isometric: the two diagonal families of the diamond lattice. A line of constant
    // u = x/halfW + y/halfH runs one way, constant v = x/halfW - y/halfH the other; walking u
    // and v in integer steps draws the diamonds. Each line is clipped to the visible rect by
    // solving for its endpoints at minY and maxY.
    const float halfW = cell.x * 0.5f, halfH = cell.y * 0.5f;
    const float corners[4][2] = {{minX, minY}, {maxX, minY}, {minX, maxY}, {maxX, maxY}};
    float uMin = 1e30f, uMax = -1e30f, vMin = 1e30f, vMax = -1e30f;
    for (const auto& c : corners) {
        const float u = c[0] / halfW + c[1] / halfH;
        const float v = c[0] / halfW - c[1] / halfH;
        uMin = std::min(uMin, u); uMax = std::max(uMax, u);
        vMin = std::min(vMin, v); vMax = std::max(vMax, v);
    }
    if ((uMax - uMin) * 0.5f > kMaxGridLines || (vMax - vMin) * 0.5f > kMaxGridLines) return;

    // A diamond centred on tile (a, b) has u = 2a and v = 2b, so its four edges lie on the odd u
    // and v lines. Stepping by 2 from an odd start draws tile boundaries; stepping by 1 would also
    // draw the lines through the centres, quartering every tile.
    auto firstOdd = [](float from) {
        const float f = std::floor(from);
        return std::fmod(std::fabs(f), 2.0f) == 1.0f ? f : f + 1.0f;
    };
    // For constant u: x = (u - y/halfH) * halfW. Same for v with the sign flipped.
    for (float u = firstOdd(uMin); u <= uMax; u += 2.0f) {
        painter.Line({(u - minY / halfH) * halfW, minY}, {(u - maxY / halfH) * halfW, maxY}, color, 1.0f);
    }
    for (float v = firstOdd(vMin); v <= vMax; v += 2.0f) {
        painter.Line({(v + minY / halfH) * halfW, minY}, {(v + maxY / halfH) * halfW, maxY}, color, 1.0f);
    }
}

void ViewportEditor::HandleViewportClick(ISceneService& sceneService, IEditorService& editorService, const CameraView& view) {
    auto* renderSystem = editorService.GetViewportScene() ? editorService.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    if (!renderSystem)
        return;

    Vector2 fbPos = sceneService.ScreenToFramebuffer(Input::GetMousePosition());
    auto* world = editorService.GetWorld();
    auto hits = world ? PickTargets(*world, *renderSystem, renderSystem->Pick(fbPos, view)) : std::vector<Entity>{};
    // A locked layer is click-through: its entities can't be selected, so you can work on what
    // is behind them without fighting the selection.
    hits.erase(std::remove_if(hits.begin(), hits.end(),
                              [&](Entity e) { return editorService.IsEntityLocked(e); }),
               hits.end());

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

// Right-click: everything under the cursor, in click order, to pick from when entities overlap.
void ViewportEditor::OpenPickMenu(ISceneService& sceneService, IEditorService& editorService, const CameraView& view) {
    auto* renderSystem = editorService.GetViewportScene() ? editorService.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    if (!renderSystem) return;
    auto* world = editorService.GetWorld();
    pickMenuHits_ = world ? PickTargets(*world, *renderSystem, renderSystem->Pick(sceneService.ScreenToFramebuffer(Input::GetMousePosition()), view))
                          : std::vector<Entity>{};
    pickMenuHits_.erase(std::remove_if(pickMenuHits_.begin(), pickMenuHits_.end(),
                                       [&](Entity e) { return editorService.IsEntityLocked(e); }),
                        pickMenuHits_.end());
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

    OverlayPainter painter(drawList, [&](Vector2 p) { return project(p, true); }, view.zoom, Theme().OverlayLineWidth);
    DrawGrid(editorService, view, imageScreenRect, painter);
    SpatialOverlayOptions overlays = overlays_;
    overlays.navAreas = overlays.navAreas && !navMeshTool_.IsActive();
    DrawSpatialOverlays(*world, editorService, overlays, painter);
    if (auto* scene = editorService.GetViewportScene()) {
        navMeshTool_.DrawOverlay(*world, editorService, scene->GetSystem<Systems::NavMeshSystem>(), painter);
    }
    if (layerDrawer_.PaintMode()) {
        const ViewportInput preview{
            .mouseWorld = Systems::RenderProjector::FramebufferToWorld(sceneService.ScreenToFramebuffer(Input::GetMousePosition()), view),
            .worldPerPixel = 1.0f / view.zoom,
            .hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows),
        };
        prefabPainter_.DrawOverlay(editorService, preview, painter);
    }

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

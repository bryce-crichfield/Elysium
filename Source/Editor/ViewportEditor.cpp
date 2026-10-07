#include "ViewportEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <optional>
#include <unordered_map>
#include <ImGuizmo.h>
#include "Core/Application.h"
#include "Interfaces/IApplicationService.h"
#include "Editor/EditorApplication.h"
#include "Interfaces/ISceneService.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Core/ComponentRegistry.h"
#include "Core/EntitySerializer.h"
#include "Core/World.h"
#include "Editor/Commands/EditorCommands.h"
#include "Editor/Tools/PaintTool.h"
#include "Editor/Tools/SelectTool.h"
#include "Editor/Tools/VertexTool.h"
#include "Core/PrefabInstance.h"
#include "Core/Components/CameraComponent.h"
#include "Core/Components/ParentComponent.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Components/LayerComponent.h"
#include "Core/Components/ModelComponent.h"
#include "Core/Scene.h"
#include "Core/Math/World3D.h"
#include <cstdio>
#include "Editor/Viewport/OverlayPainter.h"
#include "Core/Systems/NavigationSystem.h"
#include "Core/Path.h"
#include "Editor/Widgets/AssetField.h"
#include "Editor/Style/AssetStyle.h"
#include "Editor/Widgets/Widgets.h"
#include "Editor/HierarchyEditor.h"
#include "Core/Systems/RenderSystem.h"
#include "Core/Input.h"
#include "Core/Math/MathTypes.h"
#include <limits>

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

// The orthographic projection of what `view` shows, for gizmo space (ground x, ground y,
// height): through the editor camera's orbit (World3D::View) to clip space. Depth is scaled down
// only to stay inside the clip range; the gizmo just needs it invertible.
Matrix4 ViewProjection(const CameraView& view) {
    const World3D::View v = Systems::RenderProjector::View3D(view);
    const float sx = 2.0f / view.viewport.width, sy = -2.0f / view.viewport.height, sz = -1.0e-5f;
    Matrix4 p;
    auto row = [&](int r, const float* coeffs, float scale, float offset) {
        p.m[0 + r] = coeffs[0] * scale;                          // ground x
        p.m[4 + r] = coeffs[2] * World3D::kGroundDepth * scale;  // ground y
        p.m[8 + r] = coeffs[1] * scale;                          // height
        p.m[12 + r] = coeffs[3] * scale + offset;
    };
    row(0, v.rowX, sx, -1.0f);
    row(1, v.rowY, sy, 1.0f);
    row(2, v.rowDepth, sz, 0.0f);
    p.m[3] = p.m[7] = p.m[11] = 0.0f;
    p.m[15] = 1.0f;
    return p;
}

// The Move gizmo works in true 3D (GL: x, up, ground depth), where the iso picture's ground is
// square, so its arrows run straight and a drag along one stays on it. ImGuizmo needs the real
// camera for that: it drags an arrow on the plane through it that faces the camera, which it
// takes from the view matrix. ImGuizmo casts its ray from clip depth 0 to 1, starting at the end
// nearer the eye, and mirrors a plane hit behind that start (fabsf), which made the gizmo leap
// for anything nearer the camera than the start. So the eye sits kEyeBack in front of `at` (the
// gizmo, GL space), clip 0 is the eye and clip 1 is as far behind it: whatever is being dragged
// is always well inside.
void MoveGizmoCamera(const CameraView& view, Vector3 at, Matrix4& viewMatrix, Matrix4& projection) {
    const World3D::View v = Systems::RenderProjector::View3D(view);
    constexpr float kEyeBack = 1.0e4f;
    const float halfW = view.viewport.width * 0.5f, halfH = view.viewport.height * 0.5f;
    for (int i = 0; i < 3; ++i) {
        viewMatrix.m[i * 4 + 0] = v.rowX[i] / v.zoom;
        viewMatrix.m[i * 4 + 1] = -v.rowY[i] / v.zoom;
        viewMatrix.m[i * 4 + 2] = v.rowDepth[i] / v.zoom;
        viewMatrix.m[i * 4 + 3] = 0.0f;
    }
    const float atDepth = v.rowDepth[0] * at.x + v.rowDepth[1] * at.y + v.rowDepth[2] * at.z;
    viewMatrix.m[12] = (v.rowX[3] - halfW) / v.zoom;
    viewMatrix.m[13] = -(v.rowY[3] - halfH) / v.zoom;
    viewMatrix.m[14] = -atDepth / v.zoom - kEyeBack;
    viewMatrix.m[15] = 1.0f;

    projection = Matrix4{};
    projection.m[0] = v.zoom / halfW;
    projection.m[5] = v.zoom / halfH;
    projection.m[10] = -0.5f / kEyeBack;  // view z 0..-2*kEyeBack -> clip 0..1
    projection.m[14] = 0.0f;
    projection.m[15] = 1.0f;
}

// The grid's two lattice steps on the ground (2D world units): the diamond's edges for an
// isometric grid, the cell's sides for a square one.
void GridAxes(const GridSettings& grid, Vector2& a, Vector2& b) {
    const Vector2 cell = grid.Cell();
    if (grid.lattice == GridLattice::Isometric) {
        a = {cell.x * 0.5f, cell.y * 0.5f};
        b = {cell.x * 0.5f, -cell.y * 0.5f};
    } else {
        a = {cell.x, 0.0f};
        b = {0.0f, cell.y};
    }
}

// `p` in lattice coordinates (multiples of a and b), and back.
Vector2 ToLattice(Vector2 p, Vector2 a, Vector2 b) {
    const float det = a.x * b.y - a.y * b.x;
    if (std::fabs(det) < 1e-6f) return p;
    return {(p.x * b.y - p.y * b.x) / det, (a.x * p.y - a.y * p.x) / det};
}
Vector2 FromLattice(Vector2 l, Vector2 a, Vector2 b) { return {l.x * a.x + l.y * b.x, l.x * a.y + l.y * b.y}; }

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

// With the editor camera turned, a World3D entity isn't drawn on the ground plane: a model is
// its box seen from the camera, anything else a screen-facing card at its anchor (its topmost
// ancestor with a Transform, see RenderSystem's Render3D). The framebuffer rect it covers, or
// nothing for an entity on the ground plane. `bounds` is its 2D (default view) bounds.
std::optional<Rectangle> TurnedOutline(const World& world, EditorApplication& editor, Entity entity,
                                       const CameraView& view, Rectangle bounds) {
    auto* scene = editor.GetViewportScene();
    if (!scene || !world.HasComponent<LayerComponent>(entity)) return std::nullopt;
    const SceneLayer* layer = scene->GetLayer(world.GetComponent<LayerComponent>(entity).name);
    if (!layer || layer->space != SceneLayerSpace::World3D) return std::nullopt;
    const World3D::View v = Systems::RenderProjector::View3D(view);

    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    auto add = [&](Vector2 p) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    };
    if (world.HasComponent<ModelComponent>(entity) && world.HasComponent<TransformComponent>(entity)) {
        const auto& component = world.GetComponent<ModelComponent>(entity);
        if (!component.loaded || !component.loaded->native) return std::nullopt;
        Vector3 lo, hi;
        World3D::ModelBounds(World3D::ModelMatrix(world.GetComponent<TransformComponent>(entity), component, *component.loaded),
                             *component.loaded, lo, hi);
        for (int corner = 0; corner < 8; ++corner) {
            const Vector3 p = v.Project({corner & 1 ? hi.x : lo.x, corner & 2 ? hi.y : lo.y, corner & 4 ? hi.z : lo.z});
            add({p.x, p.y});
        }
    } else {
        Entity anchor = entity;
        while (world.GetParent(anchor) != INVALID_ENTITY && world.HasComponent<TransformComponent>(world.GetParent(anchor))) {
            anchor = world.GetParent(anchor);
        }
        if (!world.HasComponent<TransformComponent>(anchor)) return std::nullopt;
        const auto& t = world.GetComponent<TransformComponent>(anchor);
        const Vector2 at = v.WorldToFramebuffer(t.worldX, t.worldY, t.worldZ);
        // The card is drawn 1:1 around its anchor's ground position.
        add({at.x + (bounds.x - t.worldX) * v.zoom, at.y + (bounds.y - t.worldY) * v.zoom});
        add({at.x + (bounds.x + bounds.width - t.worldX) * v.zoom, at.y + (bounds.y + bounds.height - t.worldY) * v.zoom});
    }
    return Rectangle{minX, minY, maxX - minX, maxY - minY};
}

// Whether a handle along GL direction `axis` is worth showing from this camera: not when the
// axis points (nearly) at the eye, where it shrinks to a stub that drags erratically. Within
// ~25 degrees of the view direction counts as pointing at it.
bool AxisFacesSideways(const World3D::View& view, Vector3 axis) {
    const Vector3 eye = view.TowardCamera();
    const float eyeLength = std::sqrt(eye.x * eye.x + eye.y * eye.y + eye.z * eye.z);
    const float axisLength = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (eyeLength <= 0.0f || axisLength <= 0.0f) return true;
    const float alignment = std::fabs(eye.x * axis.x + eye.y * axis.y + eye.z * axis.z) / (eyeLength * axisLength);
    return alignment < 0.9f;
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

ViewportEditor::ViewportEditor(EditorApplication& editor) : Editor(editor, Title) {
    // Order is the toolbar order and the 1-4 shortcuts. Select is first because it is the
    // default and what an unavailable tool falls back to.
    // Order is the number-key order, and the select family comes first: Select, then the three
    // that add a gizmo to it, then the tools that own the mouse themselves.
    tools_.push_back(std::make_unique<SelectTool>(GizmoMode::None));
    tools_.push_back(std::make_unique<SelectTool>(GizmoMode::Move));
    tools_.push_back(std::make_unique<SelectTool>(GizmoMode::Rotate));
    tools_.push_back(std::make_unique<SelectTool>(GizmoMode::Scale));
    tools_.push_back(std::make_unique<VertexTool>());
    tools_.push_back(std::make_unique<PaintTool>());
}

ViewportTool* ViewportEditor::ActiveTool() {
    return activeTool_ >= 0 && activeTool_ < (int)tools_.size() ? tools_[activeTool_].get() : nullptr;
}

int ViewportEditor::ToolIndex(const char* name) const {
    for (int i = 0; i < (int)tools_.size(); i++) {
        if (std::strcmp(tools_[i]->Name(), name) == 0) return i;
    }
    return -1;
}

void ViewportEditor::SetActiveTool(int index, EditorApplication& editor) {
    if (index < 0 || index >= (int)tools_.size() || index == activeTool_) return;
    if (auto* previous = ActiveTool()) previous->OnDeactivate(editor);
    activeTool_ = index;
    tools_[activeTool_]->OnActivate(editor);
    // A tool with settings opens its panel, so picking the paint tool puts its brush in front of
    // you rather than leaving you to find the panel that holds it.
    if (!tools_[activeTool_]->Parameters().Empty()) toolPanel_.SetOpen(true);
}

void ViewportEditor::RefreshToolAvailability(EditorApplication& editor, bool isScene) {
    unavailable_.assign(tools_.size(), nullptr);
    for (size_t i = 0; i < tools_.size(); i++) unavailable_[i] = tools_[i]->Unavailable(editor, isScene);

    // A tool that has become unusable (the tab is a prefab, its selection went away) is stepped
    // away from rather than left active and silently doing nothing. Note this is why Unavailable
    // must never report something the tool's own panel is how you fix.
    if (activeTool_ != kSelectTool && unavailable_[activeTool_]) SetActiveTool(kSelectTool, editor);
}

void ViewportEditor::DrawToolButtons(EditorApplication& editor, bool isScene) {
    RefreshToolAvailability(editor, isScene);

    // A rule between the select family and the tools that own the mouse themselves, so seven icons
    // in a row read as two groups. Drawn off PicksEntities() rather than a hard-coded index, because
    // that is the same property that decides who gets the right-click pick menu -- the grouping is
    // the distinction, not a coincidence of ordering.
    bool ruled = false;
    for (int i = 0; i < (int)tools_.size(); i++) {
        ViewportTool& tool = *tools_[i];
        if (!tool.PicksEntities() && !ruled) {
            ruled = true;
            ToolbarSeparator();
        } else {
            ImGui::SameLine();
        }
        if (GatedToggleIconButton(tool.Icon(), activeTool_ == i, tool.Tooltip(), unavailable_[i])) {
            SetActiveTool(i, editor);
        }
    }

    // The active tool's status line, colored here rather than by the tool, so a hint from the paint
    // tool and one from the navmesh tool read identically.
    ViewportTool* active = ActiveTool();
    if (!active) return;
    const ToolStatus status = active->Status(editor);
    if (status.text.empty()) return;

    const auto& palette = Palette();
    const ImVec4& color = status.level == ToolStatusLevel::Working ? palette.Accent
                        : status.level == ToolStatusLevel::Warning ? palette.Warning
                                                                  : palette.TextMuted;
    ToolbarSeparator();
    ImGui::AlignTextToFramePadding();
    ColoredText(color, status.text.c_str());
}

void ViewportEditor::HandleToolShortcuts(EditorApplication& editor, bool isScene) {
    if (ImGui::GetIO().WantTextInput) return;

    // Availability was already refreshed by the toolbar, which draws before the world.
    (void)isScene;
    for (int i = 0; i < (int)tools_.size() && i < 9 && i < (int)unavailable_.size(); i++) {
        if (!ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i), false)) continue;
        if (!unavailable_[i]) SetActiveTool(i, editor);
    }
    // Esc always lands on Select, so there is one key that reliably gets you out of a mode.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && activeTool_ != kSelectTool) {
        SetActiveTool(kSelectTool, editor);
    }
}

void ViewportEditor::Draw() {
    Profile;

    auto& sceneService = services_.Get<ISceneService>();
    auto& editor = editor_;

    InitializeEditorCameraIfNeeded(editor);

    // Opening a scene or switching document tabs: bring the Hierarchy forward (over Scenes)
    // so the newly shown world's entities are what's on screen.
    if (Scene* shown = editor.GetViewportScene(); shown != lastViewportScene_) {
        if (shown) ImGui::SetWindowFocus(HierarchyEditor::Title);
        lastViewportScene_ = shown;
    }
    ImGuizmo::BeginFrame();

    // No title tab: the document tabs are the header.
    if (BeginWindow(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, false)) {
        DrawDocumentTabs(sceneService, editor);
        const EditorDocument* document = editor.GetActiveDocumentInfo();
        ContentPane* pane = document && !document->HasWorld() ? PaneFor(editor, *document) : nullptr;

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            Save(editor, pane);
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

        DrawToolbar(editor, document, pane);
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, bandBottom + style.ItemSpacing.y));

        if (pane) pane->Draw();
        else if (document && document->HasWorld() && settingsOpen_.count(document->fullPath)) {
            // Settings edit the document in place.
            auto& doc = const_cast<EditorDocument&>(*document);
            if (doc.IsScene()) sceneSettings_.Draw(*doc.scene);
            else prefabSettings_.Draw(doc);
        } else DrawWorld(sceneService, editor);
    }
    EndWindow();

    // Ctrl+S is the Viewport's own (above); these apply wherever focus is.
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S)) BeginSaveAs();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) BeginNew();
    HandleFileDialog(editor);
}

void ViewportEditor::SaveActive() {
    auto& editor = editor_;
    const EditorDocument* document = editor.GetActiveDocumentInfo();
    if (document) Save(editor, document->HasWorld() ? nullptr : PaneFor(editor, *document));
}

void ViewportEditor::BeginSaveAs() {
    if (const EditorDocument* document = editor_.GetActiveDocumentInfo()) {
        fileDialog_.OpenSaveAs(document->kind, document->fullPath);
    }
}

void ViewportEditor::BeginNew() { fileDialog_.OpenNew(); }

void ViewportEditor::HandleFileDialog(EditorApplication& editor) {
    const auto result = fileDialog_.Draw();
    if (!result) return;
    if (result->mode == AssetFileDialog::Mode::New) {
        editor.CreateAsset(result->kind, result->fullPath);
        return;
    }
    // Save As: scenes and prefabs are written by EditorApplication, other kinds by their pane.
    const EditorDocument* document = editor.GetActiveDocumentInfo();
    if (!document) return;
    if (document->HasWorld()) {
        editor.SaveActiveDocumentAs(result->fullPath);
    } else if (ContentPane* pane = PaneFor(editor, *document); pane && pane->SaveAs(result->fullPath)) {
        editor.ReplaceActiveDocument(result->fullPath);
    }
}

void ViewportEditor::DrawWorld(ISceneService& sceneService, EditorApplication& editor) {
    // The image runs edge to edge under the padded toolbar.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    // Room left under the image for the footer, which is outside the image child so its clicks
    // are never viewport clicks.
    const float footerHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ViewportImage", ImVec2(0, -footerHeight), ImGuiChildFlags_None,
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
    if (editor.GetWorld() && ImGui::BeginDragDropTarget()) {
        droppedPrefab = AcceptAssetDrop(AssetKind::Prefab);
        ImGui::EndDragDropTarget();
    }
    if (!editor.GetViewportScene()) {
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
    auto& editorCam = editor.GetEditorCamera();
    CameraView view{
        editorCam.position,
        editorCam.zoom != 0.0f ? editorCam.zoom : 1.0f,
        Rectangle{0, 0, (float)fb.Width(), (float)fb.Height()}
    };
    view.yaw = editorCam.yaw;
    view.pitch = editorCam.pitch;
    if (auto* scene = editor.GetViewportScene()) {
        if (auto* renderSystem = scene->GetSystem<Systems::RenderSystem>()) {
            renderSystem->PlaceScreenInWorld(view);
            // A prefab has no game screen to lay out: its screen pixels sit at the origin,
            // 1:1, so a UI prefab's root at (0, 0) is where the world's origin is.
            if (const EditorDocument* doc = editor.GetActiveDocumentInfo(); doc && doc->IsPrefab()) {
                view.screenOrigin = {0.0f, 0.0f};
                view.screenScale = 1.0f;
            }
            // The layer drawer's hide/solo, re-pushed every frame. The sorter applies it to its
            // own copy of the layer list, so the scene's SceneLayer flags stay untouched.
            renderSystem->SetHiddenLayerOverride(editor.GetHiddenLayers());
            renderSystem->SetUnlitOverride(!lightingOn_);
            renderSystem->SetViewOverride(view);
        }
    }

    // A prefab dropped from the Assets panel is placed where it was dropped.
    if (droppedPrefab) PlaceDroppedPrefab(sceneService, editor, view, *droppedPrefab);

    DrawViewportOverlays(sceneService, editor, view, imageScreenRect);

    const Vector2 mouseFb = sceneService.ScreenToFramebuffer(Input::GetMousePosition());
    const ViewportInput input{
        .mouseWorld = Systems::RenderProjector::FramebufferToWorld(mouseFb, view),
        .mouseFb = mouseFb,
        .worldPerPixel = 1.0f / view.zoom,
        .hovered = imageHovered,
        .clicked = imageClicked,
        .rightClicked = imageRightClicked,
        .imageScreenRect = imageScreenRect,
    };

    // The gizmo comes first, and only for a tool that carries one: a paint stroke that also
    // dragged the last selection around would be unusable.
    // The camera's own controls come before anything that edits: the orientation widget, and Alt
    // with the left button, which orbits instead of clicking.
    const bool widgetOwnsMouse = editor.GetViewportScene() && DrawOrientationWidget(editor, view, imageScreenRect);
    const bool cameraOwnsMouse = widgetOwnsMouse || isOrbitingCamera_ || (imageHovered && ImGui::GetIO().KeyAlt);

    ViewportTool* tool = ActiveTool();
    const GizmoMode gizmo = tool ? tool->Gizmo() : GizmoMode::None;
    const bool gizmoOwnsMouse = !cameraOwnsMouse &&
        gizmo != GizmoMode::None && HandleGizmo(sceneService, editor, view, imageScreenRect, gizmo);

    bool toolConsumedClick = false;
    if (tool && !gizmoOwnsMouse && !cameraOwnsMouse && editor.GetWorld()) {
        ToolContext context = MakeToolContext(sceneService, editor, view, input);
        toolConsumedClick = tool->HandleInput(context);
    }

    const bool canInteract = imageHovered && !gizmoOwnsMouse && !toolConsumedClick && !widgetOwnsMouse;
    HandleEditorCameraInput(sceneService, editor, view, canInteract);
    if (canInteract) {
        HandleGizmoShortcuts(editor);
        // After the tool, so a tool that uses Escape itself (cancelling a half-laid polygon)
        // gets it before Escape means "back to Select".
        const EditorDocument* shown = editor.GetActiveDocumentInfo();
        HandleToolShortcuts(editor, shown && shown->IsScene());
        // The pick menu belongs to the tools that pick; the others use right-click themselves.
        if (imageRightClicked && tool && tool->PicksEntities()) OpenPickMenu(sceneService, editor, view);
    }
    DrawPickMenu(editor);

    // Last, so it sits over the scene and its own clicks aren't treated as viewport clicks.
    if (auto* scene = editor.GetViewportScene()) {
        const auto* document = editor.GetActiveDocumentInfo();
        if (document && document->IsScene()) layerDrawer_.Draw(*scene, editor, imageScreenRect);
    }
    // Opposite edge from the layer drawer, so both can be open at once.
    toolPanel_.Draw(ActiveTool(), imageScreenRect);

    ImGui::EndChild();

    const EditorDocument* footerDocument = editor.GetActiveDocumentInfo();
    DrawFooter(editor, footerDocument && footerDocument->IsScene());
}

// One closeable tab per open document (scene or prefab), each its own copy loaded from disk.
void ViewportEditor::DrawDocumentTabs(ISceneService& sceneService, EditorApplication& editor) {
    const auto& documents = editor.GetDocuments();
    const int active = editor.GetActiveDocument();
    // EditorApplication changed the active document (e.g. a prefab was just opened): make ImGui follow.
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

void ViewportEditor::DrawToolbar(EditorApplication& editor, const EditorDocument* document, ContentPane* pane) {
    // No play/pause: the editor never simulates its documents, so nothing a simulation
    // moved can be saved by mistake. Play mode (F2) runs the saved files instead. No Save button
    // either -- File > Save and Ctrl+S already do it, and a row of icons should only carry the
    // things that are specific to this viewport.
    if (pane) {
        pane->DrawToolbar();
        return;
    }

    // A scene or prefab can swap its world for its settings (a scene's layers and systems, a
    // prefab's parameters) and back. One button, not two: it is a toggle, so it shows the sliders
    // going in and a back arrow -- lit, like any other active toggle -- coming out. It is the only
    // thing on this bar that is not about editing, hence the separator after it.
    const bool settings = document && document->HasWorld() && settingsOpen_.count(document->fullPath);
    if (document && document->HasWorld()) {
        const char* icon = settings ? ICON_FA_ARROW_LEFT : ICON_FA_SLIDERS;
        const char* tooltip = settings ? "Back to the rendered view"
                              : document->IsScene() ? "Settings - the scene's layers and systems"
                                                    : "Settings - the prefab's parameters";
        if (ToggleIconButton(icon, settings, tooltip)) {
            if (settings) settingsOpen_.erase(document->fullPath);
            else settingsOpen_.insert(document->fullPath);
        }
    }
    if (settings) return;

    // Tools next: which one is active decides what the rest of the toolbar even means.
    const bool isSceneTab = document && document->IsScene();
    if (document && document->HasWorld()) ToolbarSeparator();
    DrawToolButtons(editor, isSceneTab);

    // Everything after the tool buttons is right-aligned, because the active tool's status line
    // sits between them and is a different length for every tool and every state -- anything left
    // floating after it moved around as you worked, which is what made the snap toggle feel loose.
    //
    // The focused layer used to be named here too. It read as a button -- it looked like one and did
    // nothing when clicked -- and the layer drawer already shows which layer has focus, so it is gone.
    GridSettings& grid = editor.GetGrid();
    auto& camera = editor.GetEditorCamera();
    char readout[64];
    snprintf(readout, sizeof(readout), "%.0f, %.0f   %.0f%%", camera.position.x, camera.position.y, camera.zoom * 100.0f);

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    AlignRight(ButtonWidth(ICON_FA_BORDER_ALL) + ToolbarSeparatorWidth()
               + ImGui::CalcTextSize(readout).x + spacing + ButtonWidth(ICON_FA_CROSSHAIRS));

    // Snap is the one grid control that belongs up here: it changes what an edit does, not what you
    // can see. Grid visibility is a display setting, so it went to the footer with the rest of them.
    if (ToggleIconButton(ICON_FA_BORDER_ALL, grid.snapEnabled, "Snap to grid")) grid.snapEnabled = !grid.snapEnabled;

    // Editor camera readout, and a reset back to the scene's camera.
    ToolbarSeparator();
    ImGui::AlignTextToFramePadding();
    ColoredText(Palette().TextMuted, readout);
    ImGui::SameLine();
    if (IconButton(ICON_FA_CROSSHAIRS, "Reset view to the scene camera")) camera.initialized = false;
}


// What is shown, as opposed to what the mouse does. Splitting the two is the point: the top bar had
// grown the panel toggles, an overlays popup and the gizmo modes in among the tool buttons, so a row
// that should answer "what am I editing with" also answered "what can I see".
void ViewportEditor::DrawFooter(EditorApplication& editor, bool isSceneTab) {
    // Its own id scope. An ImGui button's id is its label, the footer shares a window with the
    // toolbar, and icons repeat across the two by design -- the nav-area overlay toggle and the
    // navmesh tool are both ICON_FA_ROUTE, and without this they are one widget fighting itself.
    ImGui::PushID("ViewportFooter");
    ViewportTool* tool = ActiveTool();
    toolPanel_.DrawToolbarButton(tool && !tool->Parameters().Empty() ? nullptr : "This tool has no settings");
    ImGui::SameLine();
    layerDrawer_.DrawToolbarButton(isSceneTab ? nullptr : "A prefab has no layers");

    // Panels on one side of the rule, what the viewport draws on the other.
    ToolbarSeparator();
    GridSettings& grid = editor.GetGrid();
    if (ToggleIconButton(ICON_FA_TABLE_CELLS_LARGE, grid.showGrid, "Show grid")) grid.showGrid = !grid.showGrid;
    ImGui::SameLine();
    if (ToggleIconButton(ICON_FA_LIGHTBULB, lightingOn_, lightingOn_ ? "Lighting (on)" : "Lighting (off: drawn fully lit)")) {
        lightingOn_ = !lightingOn_;
    }
    ImGui::SameLine();
    DrawOverlayToggles(overlays_);
    ImGui::PopID();
}

void ViewportEditor::Save(EditorApplication& editor, ContentPane* pane) {
    if (pane) pane->Save();
    else editor.SaveActiveDocument();
}

ContentPane* ViewportEditor::PaneFor(EditorApplication& editor, const EditorDocument& document) {
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
        case AssetKind::Model: return MakeModelPane(services, document);
        case AssetKind::Sound: return MakeSoundPane(services, document);
        default: return nullptr;
    }
}

void ViewportEditor::HandleGizmoShortcuts(EditorApplication& editor) {
    // W/E/R survive as aliases for the move, rotate and scale tools -- the same keys as before,
    // now selecting a tool rather than setting a mode on one.
    if (ImGui::GetIO().WantTextInput || ImGui::GetIO().KeyCtrl) return;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) SetActiveTool(kMoveTool, editor);
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) SetActiveTool(kRotateTool, editor);
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) SetActiveTool(kScaleTool, editor);
}

void ViewportEditor::InitializeEditorCameraIfNeeded(EditorApplication& editor) {
    auto* world = editor.GetWorld();
    if (world != lastWorld_) {
        lastWorld_ = world;
        editor.GetEditorCamera().initialized = false;
    }

    auto& cam = editor.GetEditorCamera();
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

void ViewportEditor::HandleEditorCameraInput(ISceneService& sceneService, EditorApplication& editor,
                                             const CameraView& view, bool hovered) {
    auto& cam = editor.GetEditorCamera();
    // The view as the camera is now (this frame's earlier input may have moved it), so every
    // change below keeps what's under the cursor exactly there.
    auto current = [&] {
        CameraView now = view;
        now.position = cam.position;
        now.zoom = cam.zoom != 0.0f ? cam.zoom : 1.0f;
        now.yaw = cam.yaw;
        now.pitch = cam.pitch;
        return now;
    };

    if (hovered && Input::IsMouseButtonPressed(MouseButton::Middle)) isPanningCamera_ = true;
    if (!Input::IsMouseButtonDown(MouseButton::Middle)) isPanningCamera_ = false;
    if (hovered && ImGui::GetIO().KeyAlt && Input::IsMouseButtonPressed(MouseButton::Left)) isOrbitingCamera_ = true;
    if (!Input::IsMouseButtonDown(MouseButton::Left)) isOrbitingCamera_ = false;

    // Not on the press itself: its delta is wherever the mouse was before.
    if (isOrbitingCamera_ && !Input::IsMouseButtonPressed(MouseButton::Left)) {
        // About the ground point at the middle of the view, which the orbit keeps fixed.
        const Vector2 delta = Input::GetMouseDelta();
        cam.yaw += delta.x * 0.4f;
        cam.pitch = std::clamp(cam.pitch + delta.y * 0.3f, 10.0f, 89.0f);
        cam.targetYaw = cam.yaw;
        cam.targetPitch = cam.pitch;
    } else {
        // Ease toward the orientation widget's last snap.
        const float step = std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
        auto ease = [&](float& value, float target) {
            value += (target - value) * step;
            if (std::fabs(target - value) < 0.05f) value = target;
        };
        ease(cam.yaw, cam.targetYaw);
        ease(cam.pitch, cam.targetPitch);
    }
    // Once settled, back into -180..180 (the same view), so the default view is exactly yaw 0.
    if (cam.yaw == cam.targetYaw && std::fabs(cam.yaw) > 180.0f) {
        cam.yaw = cam.targetYaw = WrapDegrees(cam.yaw);
    }

    if (isPanningCamera_) {
        // The ground under the cursor follows it.
        const CameraView now = current();
        const Vector2 mouse = sceneService.ScreenToFramebuffer(Input::GetMousePosition());
        const Vector2 delta = Input::GetMouseDelta();
        const Vector2 to = Systems::RenderProjector::FramebufferToWorld(mouse, now);
        const Vector2 from = Systems::RenderProjector::FramebufferToWorld({mouse.x - delta.x, mouse.y - delta.y}, now);
        cam.position.x -= to.x - from.x;
        cam.position.y -= to.y - from.y;
        return;
    }

    float wheel = hovered ? Input::GetMouseWheelMove() : 0.0f;
    if (wheel != 0.0f) {
        // The ground point under the cursor before the zoom, then move the camera so the
        // same point is under it after.
        const Vector2 mouseFbPos = sceneService.ScreenToFramebuffer(Input::GetMousePosition());
        const Vector2 before = Systems::RenderProjector::FramebufferToWorld(mouseFbPos, current());
        cam.zoom = std::clamp(cam.zoom * (1.0f + wheel * 0.1f), 0.1f, 10.0f);
        const Vector2 after = Systems::RenderProjector::FramebufferToWorld(mouseFbPos, current());
        cam.position.x += before.x - after.x;
        cam.position.y += before.y - after.y;
    }
}

bool ViewportEditor::DrawOrientationWidget(EditorApplication& editor, const CameraView& view, Rectangle imageScreenRect) {
    auto& cam = editor.GetEditorCamera();
    constexpr float kRadius = 40.0f, kBall = 9.0f, kMargin = 14.0f;
    const float right = imageScreenRect.x + imageScreenRect.width - (layerDrawer_.IsOpen() ? 260.0f : 0.0f);
    const ImVec2 center{right - kMargin - kRadius - kBall, imageScreenRect.y + kMargin + kRadius + kBall};
    const ImVec2 home{center.x, center.y + kRadius + kBall + 14.0f};

    // Each axis as the camera sees it: its GL direction through the view's rotation, on screen
    // and in depth. Ground x is GL x, ground y is GL z, up is GL y.
    const World3D::View v = Systems::RenderProjector::View3D(view);
    struct Axis {
        Vector3 gl;
        const char* label;
        ImVec4 color;
        float yaw;    // the yaw that looks from this side, or NaN to keep the yaw
        float pitch;  // the pitch to ease to, or NaN for a ball that can't be clicked
        ImVec2 at{};
        float depth = 0.0f;
    };
    const float nan = std::numeric_limits<float>::quiet_NaN();
    // Looking from a side keeps the tilt, unless it's from above, which comes back to default.
    const float sidePitch = cam.targetPitch > 80.0f ? World3D::kDefaultPitch : cam.targetPitch;
    Axis axes[] = {
        {{1, 0, 0}, "X", Palette().AxisX, -90.0f, sidePitch},
        {{-1, 0, 0}, "", Palette().AxisX, 90.0f, sidePitch},
        {{0, 0, 1}, "Y", Palette().AxisY, 0.0f, sidePitch},
        {{0, 0, -1}, "", Palette().AxisY, 180.0f, sidePitch},
        {{0, 1, 0}, "Z", Palette().Accent, nan, 89.0f},
        {{0, -1, 0}, "", Palette().Accent, nan, nan},
    };
    for (Axis& axis : axes) {
        const float sx = (v.rowX[0] * axis.gl.x + v.rowX[1] * axis.gl.y + v.rowX[2] * axis.gl.z) / v.zoom;
        const float sy = (v.rowY[0] * axis.gl.x + v.rowY[1] * axis.gl.y + v.rowY[2] * axis.gl.z) / v.zoom;
        axis.depth = (v.rowDepth[0] * axis.gl.x + v.rowDepth[1] * axis.gl.y + v.rowDepth[2] * axis.gl.z) / v.zoom;
        axis.at = {center.x + sx * kRadius, center.y + sy * kRadius};
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    auto within = [&](ImVec2 p, float r) { return (mouse.x - p.x) * (mouse.x - p.x) + (mouse.y - p.y) * (mouse.y - p.y) <= r * r; };
    const bool overWidget = ImGui::IsWindowHovered() && within(center, kRadius + kBall + 2.0f);
    const bool overHome = ImGui::IsWindowHovered() && within(home, 7.0f);

    // The nearest clickable ball under the mouse.
    int hot = -1;
    for (int i = 0; i < 6; ++i) {
        if (!overWidget || std::isnan(axes[i].pitch) || !within(axes[i].at, kBall + 4.0f)) continue;
        if (hot < 0 || axes[i].depth > axes[hot].depth) hot = i;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (overWidget || isOrbitingCamera_) {
        draw->AddCircleFilled(center, kRadius + kBall + 2.0f, Palette().ToU32(Palette().WithAlpha(Palette().Base, 0.55f)));
    }
    int order[6] = {0, 1, 2, 3, 4, 5};
    std::sort(order, order + 6, [&](int a, int b) { return axes[a].depth < axes[b].depth; });
    for (int i : order) {
        const Axis& axis = axes[i];
        const bool positive = axis.label[0] != 0;
        // Axes pointing away from the camera are dimmer.
        const float shade = axis.depth < -0.01f ? 0.6f : 1.0f;
        ImVec4 color = axis.color;
        color.x *= shade; color.y *= shade; color.z *= shade;
        if (positive) draw->AddLine(center, axis.at, Palette().ToU32(color), 2.0f);
        const ImVec4 fill = i == hot ? Palette().Selection : positive ? color : Palette().WithAlpha(color, 0.45f);
        draw->AddCircleFilled(axis.at, positive ? kBall : kBall * 0.7f, Palette().ToU32(fill));
        if (positive) {
            const ImVec2 size = ImGui::CalcTextSize(axis.label);
            draw->AddText({axis.at.x - size.x * 0.5f, axis.at.y - size.y * 0.5f}, IM_COL32(15, 15, 20, 255), axis.label);
        }
    }
    const ImVec4 homeColor = overHome ? Palette().Selection : Palette().TextMuted;
    draw->AddCircle(home, 5.0f, Palette().ToU32(homeColor), 0, 1.5f);
    if (!view.IsDefaultOrientation() || overHome) draw->AddCircleFilled(home, 2.5f, Palette().ToU32(homeColor));

    if (overHome) {
        ImGui::SetTooltip("Default view");
    } else if (hot >= 0) {
        ImGui::SetTooltip(axes[hot].gl.y > 0.0f ? "View from above" : "View from this side");
    } else if (overWidget && !isOrbitingCamera_) {
        ImGui::SetTooltip("Drag to orbit (or Alt + drag in the viewport)");
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (overHome) {
            // The whole turn nearest the current yaw, so it doesn't spin back round.
            cam.targetYaw = cam.yaw - WrapDegrees(cam.yaw);
            cam.targetPitch = World3D::kDefaultPitch;
            return true;
        }
        if (hot >= 0) {
            if (!std::isnan(axes[hot].yaw)) cam.targetYaw = cam.yaw + WrapDegrees(axes[hot].yaw - cam.yaw);
            cam.targetPitch = axes[hot].pitch;
            return true;
        }
        if (overWidget) isOrbitingCamera_ = true;  // a drag on the widget orbits
    }
    return overWidget || overHome || isOrbitingCamera_;
}

bool ViewportEditor::HandleGizmo(ISceneService& sceneService, EditorApplication& editor,
                                 const CameraView& view, Rectangle imageScreenRect, GizmoMode gizmo) {
    auto* world = editor.GetWorld();
    const auto& selected = editor.GetSelectedEntities();
    // The gizmo sits on the most recently selected entity and the rest of the selection follows
    // it by the same delta, so a box-selected row of walls moves as one.
    if (!world || selected.empty() || !world->HasComponent<TransformComponent>(selected.back())) {
        gizmoDragEntities_.clear();
        return false;
    }
    const Entity entity = selected.back();
    // Locked layer: no gizmo at all, so there's nothing to drag it by.
    if (editor.IsEntityLocked(entity)) {
        gizmoDragEntities_.clear();
        return false;
    }

    // Everything the drag should carry along: the rest of the selection, minus anything a
    // selected ancestor already moves (applying the delta twice would double its motion) and
    // anything on a locked layer.
    std::vector<Entity> followers;
    for (Entity other : selected) {
        if (other == entity || !world->IsAlive(other)) continue;
        if (!world->HasComponent<TransformComponent>(other)) continue;
        if (editor.IsEntityLocked(other)) continue;
        const bool carriedByAncestor = std::any_of(selected.begin(), selected.end(), [&](Entity ancestor) {
            return ancestor != other && world->IsAncestorOf(ancestor, other);
        });
        if (!carriedByAncestor) followers.push_back(other);
    }

    // A Screen2D entity's position is a game-screen pixel, which the editor shows in the
    // world at view.screenOrigin + p * screenScale; the gizmo works in that world position.
    auto* renderSystem = editor.GetViewportScene() ? editor.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    const bool isWorldSpace = renderSystem ? renderSystem->GetEntityRenderInfo(entity).isWorldSpace : true;
    const float screenScale = view.screenScale != 0.0f ? view.screenScale : 1.0f;
    auto& t = world->GetComponent<TransformComponent>(entity);

    // On a World3D layer the gizmo is 3D: it sits at the entity's height and drags it (z) too.
    bool is3D = false;
    if (isWorldSpace && world->HasComponent<LayerComponent>(entity) && editor.GetViewportScene()) {
        const SceneLayer* layer = editor.GetViewportScene()->GetLayer(world->GetComponent<LayerComponent>(entity).name);
        is3D = layer && layer->space == SceneLayerSpace::World3D;
    }
    auto toShown = [&](Vector2 p) {
        return isWorldSpace ? p : Vector2{ view.screenOrigin.x + p.x * screenScale, view.screenOrigin.y + p.y * screenScale };
    };
    auto fromShown = [&](Vector2 p) {
        return isWorldSpace ? p : Vector2{ (p.x - view.screenOrigin.x) / screenScale, (p.y - view.screenOrigin.y) / screenScale };
    };

    // The primary's local transform before the manipulation, so whatever delta it ends up
    // receiving can be handed on to the followers.
    const float wasX = t.localX, wasY = t.localY, wasZ = t.localZ, wasRotation = t.localRotation;
    const float wasScaleX = t.localScaleX, wasScaleY = t.localScaleY;
    Matrix4 matrix = EntityMatrix(toShown({ t.worldX, t.worldY }), t.worldRotation, t.worldScaleX, t.worldScaleY);
    if (is3D) matrix.m[14] = t.worldZ;
    Matrix4 viewMatrix;
    Matrix4 projection = ViewProjection(view);

    // Moving a world entity: the gizmo in GL space, its arrows along the grid's lattice (an
    // isometric grid's tile rows), so dragging one walks the entity along a row of tiles.
    const bool moveIn3D = gizmo == GizmoMode::Move && isWorldSpace;
    Vector2 latticeA, latticeB;
    GridAxes(editor.GetGrid(), latticeA, latticeB);
    if (moveIn3D) {

        auto glAxis = [](Vector2 ground) {
            const Vector3 gl = World3D::ToGL(ground.x, ground.y, 0.0f);
            const float length = std::sqrt(gl.x * gl.x + gl.z * gl.z);
            return length > 0.0f ? Vector3{gl.x / length, 0.0f, gl.z / length} : Vector3{1.0f, 0.0f, 0.0f};
        };
        const Vector3 ax = glAxis(latticeA), ay = glAxis(latticeB);
        const Vector3 at = World3D::ToGL(t.worldX, t.worldY, t.worldZ);
        matrix = Matrix4{};
        matrix.m[0] = ax.x; matrix.m[1] = ax.y; matrix.m[2] = ax.z;
        matrix.m[4] = ay.x; matrix.m[5] = ay.y; matrix.m[6] = ay.z;
        matrix.m[8] = 0.0f; matrix.m[9] = 1.0f; matrix.m[10] = 0.0f;  // height
        matrix.m[12] = at.x; matrix.m[13] = at.y; matrix.m[14] = at.z;
        MoveGizmoCamera(view, at, viewMatrix, projection);
    }

    ApplyGizmoStyle(Palette());
    ImGuizmo::SetOrthographic(true);
    ImGuizmo::AllowAxisFlip(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(imageScreenRect.x, imageScreenRect.y, imageScreenRect.width, imageScreenRect.height);

    // Only the handles the camera sees side-on: looking straight down there's no height arrow,
    // looking along a tile row no arrow for that row. Each axis's GL direction is the gizmo
    // matrix's column for it (Move), else the ground axes through ViewProjection's mapping
    // (ground x -> GL x, ground y -> GL depth, height -> GL up), turned by the entity (Scale).
    const World3D::View view3D = Systems::RenderProjector::View3D(view);
    const bool turned = isWorldSpace;  // a Screen2D entity's gizmo always faces the camera
    auto shows = [&](Vector3 axis) { return !turned || AxisFacesSideways(view3D, axis); };
    Vector3 axisX{1.0f, 0.0f, 0.0f}, axisY{0.0f, 0.0f, World3D::kGroundDepth};
    const Vector3 axisUp{0.0f, 1.0f, 0.0f};
    if (moveIn3D) {
        axisX = {matrix.m[0], matrix.m[1], matrix.m[2]};
        axisY = {matrix.m[4], matrix.m[5], matrix.m[6]};
    } else if (gizmo == GizmoMode::Scale) {
        const float c = cosf(t.worldRotation * DegToRad), s = sinf(t.worldRotation * DegToRad);
        axisX = {c, 0.0f, s * World3D::kGroundDepth};
        axisY = {-s, 0.0f, c * World3D::kGroundDepth};
    }
    const bool showX = shows(axisX), showY = shows(axisY);

    int moveHandles = (showX ? ImGuizmo::TRANSLATE_X : 0) | (showY ? ImGuizmo::TRANSLATE_Y : 0);
    if (is3D && shows(axisUp)) moveHandles |= ImGuizmo::TRANSLATE_Z;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION(moveHandles);
    ImGuizmo::MODE mode = moveIn3D ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    float snapValue = MoveSnap;
    if (gizmo == GizmoMode::Rotate) {
        // The one ring, about the height axis. Kept even edge-on: hiding it would leave nothing.
        operation = ImGuizmo::ROTATE_Z;
        snapValue = RotateSnap;
    } else if (gizmo == GizmoMode::Scale) {
        operation = ImGuizmo::OPERATION((showX ? ImGuizmo::SCALE_X : 0) | (showY ? ImGuizmo::SCALE_Y : 0));
        mode = ImGuizmo::LOCAL;
        snapValue = ScaleSnap;
    }
    // Never hide every handle (a camera looking down the only axis there is).
    if (operation == 0) operation = gizmo == GizmoMode::Scale ? ImGuizmo::OPERATION(ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y)
                                                              : ImGuizmo::OPERATION(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y);
    const float snap[3] = { snapValue, snapValue, snapValue };
    // Ctrl is the ad-hoc snap. A move is instead snapped to the editing grid below, since
    // ImGuizmo's snap is axis-aligned and an isometric lattice isn't.
    const bool gridSnapsMove = gizmo == GizmoMode::Move && isWorldSpace && editor.GetGrid().snapEnabled;
    const bool snapping = ImGui::GetIO().KeyCtrl && !gridSnapsMove;

    if (ImGuizmo::Manipulate(viewMatrix.m, projection.m, operation, mode, matrix.m, nullptr, snapping ? snap : nullptr)) {
        const float* m = matrix.m;
        // Each mode writes back only what it edits, so a move never rewrites (and rounds)
        // the rotation or scale. Changes land in the local transform; TransformSystem
        // recomposes world next frame.
        if (gizmo == GizmoMode::Move) {
            Vector2 target = fromShown({ m[12], m[13] });
            float targetZ = m[14];
            if (moveIn3D) {
                target = {m[12], m[14] / World3D::kGroundDepth};
                targetZ = m[13];
            }
            if (gridSnapsMove) {
                // Snap only the lattice coordinates the drag changed: dragging along one row
                // leaves the entity's place across it alone, even if that's off the grid.
                const Vector2 was = ToLattice({t.worldX, t.worldY}, latticeA, latticeB);
                Vector2 now = ToLattice(target, latticeA, latticeB);
                if (std::fabs(now.x - was.x) > 1e-3f) now.x = std::round(now.x);
                else now.x = was.x;
                if (std::fabs(now.y - was.y) > 1e-3f) now.y = std::round(now.y);
                else now.y = was.y;
                target = FromLattice(now, latticeA, latticeB);
            }
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
            // Height is parent + local, so the world delta is the local delta.
            if (is3D) t.localZ += targetZ - t.worldZ;
        } else if (gizmo == GizmoMode::Rotate) {
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

        // Hand the primary's delta to the rest of the selection. Position goes across as an
        // offset and scale as a ratio, so each entity keeps its own arrangement instead of being
        // snapped onto the one the gizmo happens to be on.
        const float deltaX = t.localX - wasX, deltaY = t.localY - wasY, deltaZ = t.localZ - wasZ;
        const float deltaRotation = t.localRotation - wasRotation;
        const float ratioX = wasScaleX != 0.0f ? t.localScaleX / wasScaleX : 1.0f;
        const float ratioY = wasScaleY != 0.0f ? t.localScaleY / wasScaleY : 1.0f;

        for (Entity follower : followers) {
            auto& other = world->GetComponent<TransformComponent>(follower);
            if (gizmo == GizmoMode::Move) {
                other.localX += deltaX;
                other.localY += deltaY;
                other.localZ += deltaZ;
            } else if (gizmo == GizmoMode::Rotate) {
                other.localRotation += deltaRotation;
            } else {
                other.localScaleX *= ratioX;
                other.localScaleY *= ratioY;
            }
        }
    }

    RecordGizmoDrag(editor, entity, followers, gizmo);
    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

void ViewportEditor::RecordGizmoDrag(EditorApplication& editor, Entity primary,
                                     const std::vector<Entity>& followers, GizmoMode mode) {
    // The gizmo writes the transform directly on every frame of a drag, and ImGuizmo exposes no
    // drag-begin or drag-end of its own. Rather than record each frame and merge them, watch the
    // edge of IsUsing() and snapshot once at each end: the entry is then exactly the drag.
    auto* world = editor.GetWorld();
    const bool dragging = ImGuizmo::IsUsing();
    const std::string tag = ComponentRegistry::Instance().GetXmlTag(TransformComponent::Name());

    if (dragging && gizmoDragEntities_.empty()) {
        gizmoDragEntities_.push_back(primary);
        gizmoDragEntities_.insert(gizmoDragEntities_.end(), followers.begin(), followers.end());
        gizmoDragBefore_.clear();
        for (Entity entity : gizmoDragEntities_) {
            gizmoDragBefore_.push_back(EntityXml::SaveComponent(*world, entity, tag));
        }
        return;
    }
    if (dragging || gizmoDragEntities_.empty()) return;

    const std::vector<Entity> dragged = std::move(gizmoDragEntities_);
    const std::vector<std::string> before = std::move(gizmoDragBefore_);
    gizmoDragEntities_.clear();
    gizmoDragBefore_.clear();

    // GizmoMode::None is 0, and a drag only ever happens under one of the other three.
    static constexpr const char* labels[] = { "Move Entity", "Rotate Entity", "Scale Entity" };
    const char* label = labels[std::clamp((int)mode - 1, 0, 2)];

    // One transaction, so dragging six walls is one Ctrl+Z rather than six.
    editor.BeginTransaction(label);
    for (size_t i = 0; i < dragged.size() && i < before.size(); i++) {
        if (!world->IsAlive(dragged[i])) continue;
        std::string after = EntityXml::SaveComponent(*world, dragged[i], tag);
        if (after == before[i]) continue;  // a click on the gizmo that moved nothing
        editor.Execute(std::make_unique<ComponentEditCommand>(
            EntityRef{ editor.StableIdOf(dragged[i]) }, tag, before[i], std::move(after),
            label));
    }
    editor.EndTransaction();
}

void ViewportEditor::PlaceDroppedPrefab(ISceneService& sceneService, EditorApplication& editor, const CameraView& view,
                                        const std::string& relativePath) {
    const Entity placed = editor.InstantiatePrefab(Path(relativePath).GetFullPath());
    World* world = editor.GetWorld();
    if (placed == INVALID_ENTITY || !world || !world->HasComponent<TransformComponent>(placed)) return;
    Vector2 target = editor.SnapToGrid(
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
void ViewportEditor::DrawGrid(EditorApplication& editor, const CameraView& view,
                              Rectangle imageScreenRect, OverlayPainter& painter) {
    constexpr int kMaxGridLines = 400;
    const GridSettings& grid = editor.GetGrid();
    if (!grid.showGrid) return;
    const Vector2 cell = grid.Cell();
    if (cell.x <= 0.0f || cell.y <= 0.0f) return;

    // The world rect around the ground the viewport currently shows (its corners, which an
    // orbiting camera turns).
    float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
    for (Vector2 corner : {Vector2{0.0f, 0.0f}, Vector2{view.viewport.width, 0.0f}, Vector2{0.0f, view.viewport.height},
                           Vector2{view.viewport.width, view.viewport.height}}) {
        const Vector2 ground = Systems::RenderProjector::FramebufferToWorld(corner, view);
        minX = std::min(minX, ground.x); maxX = std::max(maxX, ground.x);
        minY = std::min(minY, ground.y); maxY = std::max(maxY, ground.y);
    }

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

std::vector<Entity> ViewportEditor::PickAt(ISceneService&, EditorApplication& editor, const CameraView& view,
                                           Vector2 fbPos) const {
    auto* scene = editor.GetViewportScene();
    auto* renderSystem = scene ? scene->GetSystem<Systems::RenderSystem>() : nullptr;
    auto* world = editor.GetWorld();
    if (!renderSystem || !world) return {};

    std::vector<Entity> hits = PickTargets(*world, *renderSystem, renderSystem->Pick(fbPos, view));
    // A locked layer is click-through: its entities can't be selected, so you can work on what
    // is behind them without fighting the selection.
    hits.erase(std::remove_if(hits.begin(), hits.end(), [&](Entity e) { return editor.IsEntityLocked(e); }),
               hits.end());
    return hits;
}

std::vector<Entity> ViewportEditor::PickInRect(EditorApplication& editor, Rectangle worldRect) const {
    auto* scene = editor.GetViewportScene();
    auto* renderSystem = scene ? scene->GetSystem<Systems::RenderSystem>() : nullptr;
    auto* world = editor.GetWorld();
    if (!renderSystem || !world) return {};

    auto overlaps = [&](const Rectangle& a) {
        return a.x <= worldRect.x + worldRect.width && a.x + a.width >= worldRect.x &&
               a.y <= worldRect.y + worldRect.height && a.y + a.height >= worldRect.y;
    };

    std::vector<Entity> inside;
    for (Entity entity : world->GetLivingEntities()) {
        // A placement is opaque, so the box selects the placement rather than its internals.
        const Entity target = PrefabInstances::RootOf(*world, entity);
        if (target != entity) continue;
        if (editor.IsEntityLocked(target)) continue;

        const auto info = renderSystem->GetEntityRenderInfo(target);
        // Screen-space entities are excluded: the box is a world rectangle, and a HUD element's
        // position isn't in the same space.
        if (!info.isWorldSpace || !info.bounds || !overlaps(*info.bounds)) continue;
        if (std::find(inside.begin(), inside.end(), target) == inside.end()) inside.push_back(target);
    }
    return inside;
}

ToolContext ViewportEditor::MakeToolContext(ISceneService& sceneService, EditorApplication& editor,
                                            const CameraView& view, const ViewportInput& input) {
    ToolContext context{*editor.GetWorld(), editor, input};
    context.pick = [this, &sceneService, &editor, view, input] {
        return PickAt(sceneService, editor, view, input.mouseFb);
    };
    context.pickRect = [this, &editor](Rectangle worldRect) { return PickInRect(editor, worldRect); };
    context.worldToScreen = [view, input](Vector2 world) {
        const Vector2 fb = Systems::RenderProjector::WorldToFramebuffer(world, view);
        return Vector2{input.imageScreenRect.x + fb.x, input.imageScreenRect.y + fb.y};
    };
    return context;
}

// Right-click: everything under the cursor, in click order, to pick from when entities overlap.
void ViewportEditor::OpenPickMenu(ISceneService& sceneService, EditorApplication& editor, const CameraView& view) {
    auto* renderSystem = editor.GetViewportScene() ? editor.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    if (!renderSystem) return;
    auto* world = editor.GetWorld();
    pickMenuHits_ = world ? PickTargets(*world, *renderSystem, renderSystem->Pick(sceneService.ScreenToFramebuffer(Input::GetMousePosition()), view))
                          : std::vector<Entity>{};
    pickMenuHits_.erase(std::remove_if(pickMenuHits_.begin(), pickMenuHits_.end(),
                                       [&](Entity e) { return editor.IsEntityLocked(e); }),
                        pickMenuHits_.end());
    ImGui::OpenPopup("ViewportPick");
}

void ViewportEditor::DrawPickMenu(EditorApplication& editor) {
    if (!ImGui::BeginPopup("ViewportPick")) return;
    auto* world = editor.GetWorld();
    SectionHeader("Under Cursor");
    if (!world || pickMenuHits_.empty()) MutedText("Nothing here");
    for (Entity entity : pickMenuHits_) {
        if (!world || !world->IsAlive(entity)) continue;  // destroyed since the pick
        ImGui::PushID((int)entity);
        const std::string label = EntityLabel(world->GetEntityName(entity), entity);
        if (ImGui::Selectable(label.c_str(), editor.IsSelected(entity))) {
            editor.SelectEntity(entity, ImGui::GetIO().KeyShift);
        }
        ImGui::PopID();
    }
    ImGui::EndPopup();
}

void ViewportEditor::DrawViewportOverlays(ISceneService& sceneService, EditorApplication& editor,
                                           const CameraView& view, Rectangle imageScreenRect) {
    auto* world = editor.GetWorld();
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

    // A world rectangle, as the camera shows it (a turned camera turns it).
    auto drawQuad = [&](Rectangle r, bool isWorldSpace, ImU32 color) {
        const Vector2 a = project({r.x, r.y}, isWorldSpace), b = project({r.x + r.width, r.y}, isWorldSpace);
        const Vector2 c = project({r.x + r.width, r.y + r.height}, isWorldSpace), d = project({r.x, r.y + r.height}, isWorldSpace);
        const ImVec2 points[4] = {{a.x, a.y}, {b.x, b.y}, {c.x, c.y}, {d.x, d.y}};
        drawList->AddPolyline(points, 4, color, ImDrawFlags_Closed, Theme().OverlayLineWidth);
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
        drawQuad({t.worldX - halfW, t.worldY - halfH, halfW * 2.0f, halfH * 2.0f}, true, cameraGizmoColor);
    });

    OverlayPainter painter(drawList, [&](Vector3 p) {
        const Vector2 fb = Systems::RenderProjector::WorldToFramebuffer(p, view);
        return Vector2{imageScreenRect.x + fb.x, imageScreenRect.y + fb.y};
    }, view.zoom, Theme().OverlayLineWidth);
    DrawGrid(editor, view, imageScreenRect, painter);
    ViewportTool* tool = ActiveTool();
    auto* scene = editor.GetViewportScene();
    const auto* nav = scene ? scene->GetSystem<Systems::NavigationSystem>() : nullptr;
    DrawSpatialOverlays(*world, editor, nav, overlays_, painter);

    if (tool) {
        const Vector2 mouseFb = sceneService.ScreenToFramebuffer(Input::GetMousePosition());
        const ViewportInput preview{
            .mouseWorld = Systems::RenderProjector::FramebufferToWorld(mouseFb, view),
            .mouseFb = mouseFb,
            .worldPerPixel = 1.0f / view.zoom,
            .hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows),
            .imageScreenRect = imageScreenRect,
        };
        ToolContext context = MakeToolContext(sceneService, editor, view, preview);
        tool->DrawOverlay(context, painter);
    }

    // Selection highlight, sized via the entity's RenderableType::Bounds where it has one
    // (falls back to a fixed box for renderable-less/currently-culled selected entities).
    auto* renderSystem = editor.GetViewportScene() ? editor.GetViewportScene()->GetSystem<Systems::RenderSystem>() : nullptr;
    const auto& selected = editor.GetSelectedEntities();
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
        if (info.isWorldSpace && !view.IsDefaultOrientation()) {
            // A turned camera: outline where the entity is actually drawn.
            if (auto drawn = TurnedOutline(*world, editor, entity, view, *bounds)) {
                drawList->AddRect(ImVec2(imageScreenRect.x + drawn->x, imageScreenRect.y + drawn->y),
                                  ImVec2(imageScreenRect.x + drawn->x + drawn->width, imageScreenRect.y + drawn->y + drawn->height),
                                  selectionColor, 0.0f, 0, Theme().OverlayLineWidth);
                continue;
            }
        }
        drawQuad(*bounds, info.isWorldSpace, selectionColor);
    }

    drawList->PopClipRect();
}

}  // namespace Elysium

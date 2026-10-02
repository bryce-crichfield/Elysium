#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "Editor/Editor.h"
#include "Core/Entity.h"
#include "Editor/Viewport/LayerDrawer.h"
#include "Editor/Viewport/ToolPanel.h"
#include "Editor/Viewport/SpatialOverlays.h"
#include "Editor/Tools/ViewportTool.h"
#include "Editor/Widgets/AssetFileDialog.h"
#include "Editor/Panes/ContentPane.h"
#include "Editor/Settings/PrefabSettings.h"
#include "Editor/Settings/SceneSettings.h"
#include "Systems/RenderSystem.h"

namespace Elysium {
class World;
class Scene;
class EditorApplication;
struct EditorDocument;
}  // namespace Elysium

namespace Elysium::Services {
class ISceneService;
}  // namespace Elysium::Services

namespace Elysium {

// Draws the "Viewport" panel, the editor's center: one tab per open asset, colored by its
// kind, and the active one's content below a shared toolbar. Scenes and prefabs show the
// world through the free editor camera, with click-to-pick and an ImGuizmo transform gizmo
// on the selected entity (SceneService sizes its framebuffer to this panel, so the image is
// shown 1:1); a scene can swap that for its settings. Other kinds draw a ContentPane.
class ViewportEditor : public Editor {
   public:
    static constexpr const char* Title = "Viewport";

    explicit ViewportEditor(EditorApplication& editor);

    void Draw() override;

    // File menu actions, on the active tab: Save writes it back; Save As and New ask for a
    // name (and for New, a kind) first.
    void SaveActive();
    void BeginSaveAs();
    void BeginNew();

   private:
    void DrawDocumentTabs(Services::ISceneService& sceneService, EditorApplication& editor);
    void DrawToolbar(EditorApplication& editor, const EditorDocument* document, ContentPane* pane);
    // The strip under the viewport image: what is shown rather than what the mouse does. The panel
    // toggles and the overlay switches live here, so the top toolbar is only ever about editing.
    void DrawFooter(EditorApplication& editor, bool isSceneTab);
    // The rendered world of a scene or prefab tab (or the empty hint when nothing is open).
    void DrawWorld(Services::ISceneService& sceneService, EditorApplication& editor);
    void Save(EditorApplication& editor, ContentPane* pane);

    // --- Tools ---------------------------------------------------------------------------
    // Exactly one tool owns the viewport's left mouse button. Before this, paint mode and
    // navmesh mode were independent toggles that each had to remember to switch the other off,
    // and every new mode meant another branch in the input chain.
    ViewportTool* ActiveTool();
    void SetActiveTool(int index, EditorApplication& editor);
    // The index of the tool with this Name(), or -1. Lets other panels ask for a tool by name
    // rather than by a position in the registry.
    int ToolIndex(const char* name) const;
    // The tool buttons, and the active tool's status line beside them.
    void DrawToolButtons(EditorApplication& editor, bool isScene);
    // Asks every tool once per frame why it cannot be used, into `unavailable_`. Unavailable() can
    // be expensive (the vertex tool walks the selection and parses point lists), and it was being
    // asked two or three times a frame per tool by the buttons, the force-switch check and the
    // shortcut handler -- which could also disagree within one frame.
    void RefreshToolAvailability(EditorApplication& editor, bool isScene);
    // Keys 1-4 pick a tool; Esc returns to Select.
    void HandleToolShortcuts(EditorApplication& editor, bool isScene);
    void HandleFileDialog(EditorApplication& editor);
    // The pane for a tab without a world, made on first use; drops those of closed tabs.
    ContentPane* PaneFor(EditorApplication& editor, const EditorDocument& document);
    // W/E/R pick the move, rotate and scale tools, as aliases for keys 2-4.
    void HandleGizmoShortcuts(EditorApplication& editor);

    // Entities under the cursor, smallest-first, with locked layers dropped. Locked layers are
    // click-through so you can work on what sits behind them.
    std::vector<Entity> PickAt(Services::ISceneService& sceneService, EditorApplication& editor,
                               const Systems::CameraView& view, Vector2 fbPos) const;
    // Entities whose rendered bounds overlap a world rectangle, for the select tool's box drag.
    std::vector<Entity> PickInRect(EditorApplication& editor, Rectangle worldRect) const;
    // Bundles the frame's mouse state with the picking and projection the active tool may need,
    // so a tool never has to know about RenderSystem or the editor camera.
    ToolContext MakeToolContext(Services::ISceneService& sceneService, EditorApplication& editor,
                                const Systems::CameraView& view, const ViewportInput& input);

    // Turns a gizmo drag into one undo entry. ImGuizmo has no drag-begin or drag-end, so this
    // watches the edge of IsUsing() and snapshots the transform at each end of the drag.
    // `followers` are the other selected entities the drag carried along, recorded in the same
    // transaction so the whole multi-entity drag is one undo step.
    void RecordGizmoDrag(EditorApplication& editor, Entity primary,
                         const std::vector<Entity>& followers, GizmoMode mode);

    // Snaps the editor camera to the first real CameraComponent's transform/zoom the first
    // time a world becomes available, so scenes don't open centered on the origin.
    void InitializeEditorCameraIfNeeded(EditorApplication& editor);

    // Middle-mouse-drag pan + scroll-wheel zoom (anchored under the cursor) for the free
    // editor camera. Pans and zooms only start while `hovered`.
    // The orientation widget in the image's top-right corner: the world axes as the editor
    // camera sees them. Clicking an axis eases the camera round to look from that side (up:
    // from above), the dot under it back to the default view; dragging it orbits. Returns true
    // while it has the mouse.
    bool DrawOrientationWidget(EditorApplication& editor, const Systems::CameraView& view, Rectangle imageScreenRect);

    void HandleEditorCameraInput(Services::ISceneService& sceneService, EditorApplication& editor,
                                 const Systems::CameraView& view, bool hovered);

    // Draws and applies the transform gizmo on the single selected entity. Returns true while
    // the gizmo owns the mouse (hovered or dragging), so the click doesn't also re-pick.
    // `mode` comes from the active tool: move, rotate and scale are tools, not a mode of one.
    bool HandleGizmo(Services::ISceneService& sceneService, EditorApplication& editor,
                     const Systems::CameraView& view, Rectangle imageScreenRect, GizmoMode mode);
    void OpenPickMenu(Services::ISceneService& sceneService, EditorApplication& editor, const Systems::CameraView& view);
    void DrawPickMenu(EditorApplication& editor);
    void PlaceDroppedPrefab(Services::ISceneService& sceneService, EditorApplication& editor,
                            const Systems::CameraView& view, const std::string& relativePath);

    // Editor-only chrome (origin axes, camera bounds, selection outline), drawn over the
    // framebuffer image via ImGui's draw list. imageScreenRect is where the image sits on screen.
    void DrawViewportOverlays(Services::ISceneService& sceneService, EditorApplication& editor,
                               const Systems::CameraView& view, Rectangle imageScreenRect);
    // Draws the editing grid at the active document's spacing, clipped to what the viewport
    // actually shows so the line count stays bounded however far you zoom out.
    void DrawGrid(EditorApplication& editor, const Systems::CameraView& view,
                  Rectangle imageScreenRect, OverlayPainter& painter);

    SpatialOverlayOptions overlays_;
    LayerDrawer layerDrawer_;
    ToolPanel toolPanel_;

    // Named tool slots. Select is the default and what everything falls back to, and W/E/R are
    // aliases for the three that carry a gizmo, so these indices are referred to by name rather
    // than as bare numbers scattered through the file.
    static constexpr int kSelectTool = 0;
    static constexpr int kMoveTool = 1;
    static constexpr int kRotateTool = 2;
    static constexpr int kScaleTool = 3;

    std::vector<std::unique_ptr<ViewportTool>> tools_;
    int activeTool_ = kSelectTool;
    // Why each tool cannot be used this frame (null = it can), parallel to `tools_`. Filled by
    // RefreshToolAvailability at the top of the toolbar, read by everything after it.
    std::vector<const char*> unavailable_;

    // Editor camera pan drag state.
    bool isPanningCamera_ = false;
    // Editor camera orbit drag (Alt + left drag, or a drag on the orientation widget).
    bool isOrbitingCamera_ = false;
    // The footer's lightbulb: off draws every layer unlit (editor only, never saved).
    bool lightingOn_ = true;

    // The entities being dragged by the gizmo and each one's transform as the drag found it,
    // parallel arrays. Empty when no drag is in progress.
    std::vector<Entity> gizmoDragEntities_;
    std::vector<std::string> gizmoDragBefore_;
    // Height handle drag: the mouse y and the primary's z when it began.

    // Tracks world changes (e.g. loading a different scene) so the editor camera re-snaps
    // to the new scene's first camera instead of staying pointed at the old one.
    World* lastWorld_ = nullptr;

    // The document tab ImGui last showed as selected (-1 = scene stack).
    int shownDocument_ = -1;

    // Last scene the viewport showed; a change focuses the Hierarchy panel.
    Scene* lastViewportScene_ = nullptr;

    // Entities under the cursor at the last right-click, in click order.
    std::vector<Entity> pickMenuHits_;

    // Content panes of open tabs without a world, by file.
    std::unordered_map<std::string, std::unique_ptr<ContentPane>> panes_;
    // Scene and prefab tabs showing their settings instead of the world, by file.
    std::set<std::string> settingsOpen_;
    SceneSettings sceneSettings_{editor_};
    PrefabSettings prefabSettings_{editor_};
    AssetFileDialog fileDialog_;
};

}  // namespace Elysium

#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Editor.h"
#include "Core/Entity.h"
#include "Editor/LayerDrawer.h"
#include "Editor/SpatialOverlays.h"
#include "Editor/Tools/ViewportTool.h"
#include "Editor/AssetFileDialog.h"
#include "Editor/ContentPane.h"
#include "Editor/PrefabSettings.h"
#include "Editor/SceneSettings.h"
#include "Systems/RenderSystem.h"

namespace Elysium {
class World;
class Scene;
}  // namespace Elysium

namespace Elysium::Services {
class ISceneService;
class IEditorService;
struct EditorDocument;
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

    explicit ViewportEditor(ServiceLocator& services);

    void Draw() override;

    // File menu actions, on the active tab: Save writes it back; Save As and New ask for a
    // name (and for New, a kind) first.
    void SaveActive();
    void BeginSaveAs();
    void BeginNew();

   private:
    enum class GizmoMode { Move, Rotate, Scale };

    void DrawDocumentTabs(Services::ISceneService& sceneService, Services::IEditorService& editor);
    void DrawToolbar(Services::IEditorService& editor, const Services::EditorDocument* document, ContentPane* pane);
    // The rendered world of a scene or prefab tab (or the empty hint when nothing is open).
    void DrawWorld(Services::ISceneService& sceneService, Services::IEditorService& editorService);
    void Save(Services::IEditorService& editor, ContentPane* pane);

    // --- Tools ---------------------------------------------------------------------------
    // Exactly one tool owns the viewport's left mouse button. Before this, paint mode and
    // navmesh mode were independent toggles that each had to remember to switch the other off,
    // and every new mode meant another branch in the input chain.
    ViewportTool* ActiveTool();
    void SetActiveTool(int index, Services::IEditorService& editor);
    // The index of the tool with this Name(), or -1. Lets other panels ask for a tool by name
    // rather than by a position in the registry.
    int ToolIndex(const char* name) const;
    // The tool buttons, and the active tool's own controls beside them.
    void DrawToolButtons(Services::IEditorService& editor, bool isScene);
    // Keys 1-4 pick a tool; Esc returns to Select.
    void HandleToolShortcuts(Services::IEditorService& editor, bool isScene);
    void HandleFileDialog(Services::IEditorService& editor);
    // The pane for a tab without a world, made on first use; drops those of closed tabs.
    ContentPane* PaneFor(Services::IEditorService& editor, const Services::EditorDocument& document);
    // W/E/R pick the gizmo mode while the viewport is hovered and the scene is paused.
    void HandleGizmoShortcuts(Services::ISceneService& sceneService);

    // Entities under the cursor, smallest-first, with locked layers dropped. Locked layers are
    // click-through so you can work on what sits behind them.
    std::vector<Entity> PickAt(Services::ISceneService& sceneService, Services::IEditorService& editorService,
                               const Systems::CameraView& view, Vector2 fbPos) const;
    // Entities whose rendered bounds overlap a world rectangle, for the select tool's box drag.
    std::vector<Entity> PickInRect(Services::IEditorService& editorService, Rectangle worldRect) const;
    // Bundles the frame's mouse state with the picking and projection the active tool may need,
    // so a tool never has to know about RenderSystem or the editor camera.
    ToolContext MakeToolContext(Services::ISceneService& sceneService, Services::IEditorService& editorService,
                                const Systems::CameraView& view, const ViewportInput& input);

    // Turns a gizmo drag into one undo entry. ImGuizmo has no drag-begin or drag-end, so this
    // watches the edge of IsUsing() and snapshots the transform at each end of the drag.
    // `followers` are the other selected entities the drag carried along, recorded in the same
    // transaction so the whole multi-entity drag is one undo step.
    void RecordGizmoDrag(Services::IEditorService& editorService, Entity primary,
                         const std::vector<Entity>& followers);

    // Snaps the editor camera to the first real CameraComponent's transform/zoom the first
    // time a world becomes available, so scenes don't open centered on the origin.
    void InitializeEditorCameraIfNeeded(Services::IEditorService& editorService);

    // Middle-mouse-drag pan + scroll-wheel zoom (anchored under the cursor) for the free
    // editor camera. Pans and zooms only start while `hovered`.
    void HandleEditorCameraInput(Services::ISceneService& sceneService, Services::IEditorService& editorService,
                                 const Systems::CameraView& view, bool hovered);

    // Draws and applies the transform gizmo on the single selected entity. Returns true while
    // the gizmo owns the mouse (hovered or dragging), so the click doesn't also re-pick.
    bool HandleGizmo(Services::ISceneService& sceneService, Services::IEditorService& editorService,
                     const Systems::CameraView& view, Rectangle imageScreenRect);
    void OpenPickMenu(Services::ISceneService& sceneService, Services::IEditorService& editorService, const Systems::CameraView& view);
    void DrawPickMenu(Services::IEditorService& editorService);
    void PlaceDroppedPrefab(Services::ISceneService& sceneService, Services::IEditorService& editorService,
                            const Systems::CameraView& view, const std::string& relativePath);

    // Editor-only chrome (origin axes, camera bounds, selection outline), drawn over the
    // framebuffer image via ImGui's draw list. imageScreenRect is where the image sits on screen.
    void DrawViewportOverlays(Services::ISceneService& sceneService, Services::IEditorService& editorService,
                               const Systems::CameraView& view, Rectangle imageScreenRect);
    // Draws the editing grid at the active document's spacing, clipped to what the viewport
    // actually shows so the line count stays bounded however far you zoom out.
    void DrawGrid(Services::IEditorService& editorService, const Systems::CameraView& view,
                  Rectangle imageScreenRect, OverlayPainter& painter);

    GizmoMode gizmoMode_ = GizmoMode::Move;
    SpatialOverlayOptions overlays_;
    LayerDrawer layerDrawer_;

    // Index 0 is the select tool, which is the default and what everything falls back to.
    std::vector<std::unique_ptr<ViewportTool>> tools_;
    int activeTool_ = 0;

    // Editor camera pan drag state.
    bool isPanningCamera_ = false;

    // The entities being dragged by the gizmo and each one's transform as the drag found it,
    // parallel arrays. Empty when no drag is in progress.
    std::vector<Entity> gizmoDragEntities_;
    std::vector<std::string> gizmoDragBefore_;

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
    SceneSettings sceneSettings_{services_};
    PrefabSettings prefabSettings_{services_};
    AssetFileDialog fileDialog_;
};

}  // namespace Elysium

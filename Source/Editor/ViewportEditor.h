#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Editor.h"
#include "Core/Entity.h"
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
    void HandleFileDialog(Services::IEditorService& editor);
    // The pane for a tab without a world, made on first use; drops those of closed tabs.
    ContentPane* PaneFor(Services::IEditorService& editor, const Services::EditorDocument& document);
    // W/E/R pick the gizmo mode while the viewport is hovered and the scene is paused.
    void HandleGizmoShortcuts(Services::ISceneService& sceneService);

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
    void HandleViewportClick(Services::ISceneService& sceneService, Services::IEditorService& editorService, const Systems::CameraView& view);

    // Editor-only chrome (origin axes, camera bounds, selection outline), drawn over the
    // framebuffer image via ImGui's draw list. imageScreenRect is where the image sits on screen.
    void DrawViewportOverlays(Services::ISceneService& sceneService, Services::IEditorService& editorService,
                               const Systems::CameraView& view, Rectangle imageScreenRect);

    GizmoMode gizmoMode_ = GizmoMode::Move;

    // Click-cycling state: repeat-clicking the same spot advances through overlapping hits.
    Vector2 lastClickFbPos_ = { -1.0f, -1.0f };
    size_t lastClickIndex_ = 0;

    // Editor camera pan drag state.
    bool isPanningCamera_ = false;

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

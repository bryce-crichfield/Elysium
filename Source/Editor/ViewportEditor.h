#pragma once

#include "Core/Editor.h"
#include "Core/Entity.h"
#include "Systems/RenderSystem.h"

namespace Elysium {
class World;
}  // namespace Elysium

namespace Elysium::Services {
class ISceneService;
class IEditorService;
}  // namespace Elysium::Services

namespace Elysium {

// Draws the "Viewport" panel: the scene through the free editor camera, click-to-pick,
// and an ImGuizmo transform gizmo on the selected entity. SceneService sizes its
// framebuffer to this panel in the editor, so the image is shown 1:1 with no scaling.
class ViewportEditor : public Editor {
   public:
    static constexpr const char* Title = "Viewport";

    explicit ViewportEditor(ServiceLocator& services);

    void Draw() override;

   private:
    enum class GizmoMode { Move, Rotate, Scale };

    void DrawToolbar(Services::ISceneService& sceneService, Services::IEditorService& editor);
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
};

}  // namespace Elysium

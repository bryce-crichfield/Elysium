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

// Draws the "Game" viewport panel and drives viewport click-to-pick + the move gizmo —
// extracted from Application.cpp so it owns the viewport rect it needs to translate mouse
// coordinates into the framebuffer coordinates RenderSystem::Pick() expects.
class ViewportEditor : public Editor {
   public:
    explicit ViewportEditor(ServiceLocator& services);

    void Draw() override;

   private:
    void DrawToolbar(Services::ISceneService& sceneService, Services::IEditorService& editor);

    // Snaps the editor camera to the first real CameraComponent's transform/zoom the first
    // time a world becomes available, so scenes don't open centered on the origin.
    void InitializeEditorCameraIfNeeded(Services::IEditorService& editorService);

    // Middle-mouse-drag pan + scroll-wheel zoom (anchored under the cursor) for the free
    // editor camera.
    void HandleEditorCameraInput(Services::ISceneService& sceneService, Services::IEditorService& editorService);

    // Dispatches each frame to either continuing/starting a gizmo drag, or (if neither
    // applies) the ordinary click-to-pick path. Only one of the two ever runs per press.
    void HandleGizmoOrPick(Services::ISceneService& sceneService, Services::IEditorService& editorService, const Systems::CameraView& view);
    void HandleViewportClick(Services::ISceneService& sceneService, Services::IEditorService& editorService, const Systems::CameraView& view);
    void ApplyGizmoDrag(World* world, Entity entity, float zoom, Vector2 fbDelta, bool isWorldSpace);

    // Editor-only chrome (origin axes, camera gizmos, selection outline, move handle),
    // drawn on top of the already-blitted framebuffer image via ImGui's draw list.
    // imageScreenRect is the on-screen rect the framebuffer image was fit into this frame.
    void DrawViewportOverlays(Services::ISceneService& sceneService, Services::IEditorService& editorService,
                               const Systems::CameraView& view, Rectangle imageScreenRect);

    // Maps a framebuffer-space position (as returned by RenderProjector::WorldToFramebuffer)
    // to an ImGui screen-space position within imageScreenRect, for overlay drawing.
    Vector2 FramebufferToScreen(Vector2 fbPos, Rectangle imageScreenRect) const;

    // Shared between DrawViewportOverlays' move-handle visual and the gizmo hit-test below,
    // so tolerance can't drift from the visual size.
    static constexpr float kMoveHandleRadius = 8.0f;

    // Click-cycling state: repeat-clicking the same spot advances through overlapping hits.
    Vector2 lastClickFbPos_ = { -1.0f, -1.0f };
    size_t lastClickIndex_ = 0;

    // Move gizmo drag state.
    bool isDraggingGizmo_ = false;
    Entity gizmoEntity_ = INVALID_ENTITY;
    Vector2 lastGizmoFbPos_ = { 0.0f, 0.0f };
    // Cached at drag-start so the continuing-drag branch doesn't need to re-query RenderSystem.
    bool gizmoIsWorldSpace_ = true;

    // Editor camera pan drag state.
    bool isPanningCamera_ = false;

    // Tracks world changes (e.g. loading a different scene) so the editor camera re-snaps
    // to the new scene's first camera instead of staying pointed at the old one.
    World* lastWorld_ = nullptr;
};

}  // namespace Elysium

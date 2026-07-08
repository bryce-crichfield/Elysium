#pragma once

#include "Core/Editor.h"
#include "Core/Entity.h"
#include "Systems/RenderSystem.h"
#include "raylib.h"

namespace Elysium {
class World;
}  // namespace Elysium

namespace Elysium::Services {
class SceneService;
class EditorService;
}  // namespace Elysium::Services

namespace Elysium {

// Draws the "Game" viewport panel and drives viewport click-to-pick + the move gizmo —
// extracted from Application.cpp so it owns the viewport rect it needs to translate mouse
// coordinates into the framebuffer coordinates RenderSystem::Pick() expects.
class ViewportEditor : public Editor {
   public:
    ViewportEditor();

    void Draw(Application& app) override;

   private:
    void DrawToolbar(Services::SceneService& sceneService);

    // Snaps the editor camera to the first real CameraComponent's transform/zoom the first
    // time a world becomes available, so scenes don't open centered on the origin.
    void InitializeEditorCameraIfNeeded(Services::EditorService& editorService);

    // Middle-mouse-drag pan + scroll-wheel zoom (anchored under the cursor) for the free
    // editor camera.
    void HandleEditorCameraInput(Services::SceneService& sceneService, Services::EditorService& editorService);

    // Dispatches each frame to either continuing/starting a gizmo drag, or (if neither
    // applies) the ordinary click-to-pick path. Only one of the two ever runs per press.
    void HandleGizmoOrPick(Services::SceneService& sceneService, Services::EditorService& editorService, const Systems::CameraView& view);
    void HandleViewportClick(Services::SceneService& sceneService, Services::EditorService& editorService, const Systems::CameraView& view);
    void ApplyGizmoDrag(World* world, Entity entity, float zoom, Vector2 fbDelta);

    // Click-cycling state: repeat-clicking the same spot advances through overlapping hits.
    Vector2 lastClickFbPos_ = { -1.0f, -1.0f };
    size_t lastClickIndex_ = 0;

    // Move gizmo drag state.
    bool isDraggingGizmo_ = false;
    Entity gizmoEntity_ = INVALID_ENTITY;
    Vector2 lastGizmoFbPos_ = { 0.0f, 0.0f };

    // Editor camera pan drag state.
    bool isPanningCamera_ = false;

    // Tracks world changes (e.g. loading a different scene) so the editor camera re-snaps
    // to the new scene's first camera instead of staying pointed at the old one.
    World* lastWorld_ = nullptr;
};

}  // namespace Elysium

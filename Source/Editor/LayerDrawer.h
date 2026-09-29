#pragma once

#include <string>
#include "Core/MathTypes.h"

namespace Elysium {
class Scene;
namespace Services {
class IEditorService;
}

// The Viewport's layer drawer: a panel that slides out over the right edge of a scene tab,
// listing the scene's layers top-down by z. Selecting one makes it active, which filters the
// Hierarchy and is where painted prefabs land. Each row toggles lock (its entities can't be
// picked, dragged or deleted), solo (only soloed layers draw) and hidden.
//
// All of that state lives in IEditorService and is session-only, so none of it reaches the
// scene file. The drawer also holds the paint brush's prefab, since the brush is a property of
// what you're painting with, not of the scene.
class LayerDrawer {
public:
    bool IsOpen() const { return open_; }
    void SetOpen(bool open) { open_ = open; }

    // The toolbar button that opens/closes it. Returns true if the state changed.
    bool DrawToolbarButton();

    // Draws the panel inside the viewport, anchored to the right edge of `imageScreenRect`.
    // No-op while closed or when the document has no layers.
    void Draw(Scene& scene, Services::IEditorService& editor, Rectangle imageScreenRect);

    // The prefab the paint tool places, project-relative; empty when nothing is picked.
    const std::string& BrushPrefab() const { return brushPrefab_; }
    bool PaintMode() const { return paintMode_ && !brushPrefab_.empty(); }
    void SetPaintMode(bool paint) { paintMode_ = paint; }

private:
    void DrawLayerRow(Scene& scene, Services::IEditorService& editor, const std::string& name, int zIndex);
    void DrawBrushSection(Services::IEditorService& editor);

    bool open_ = false;
    bool paintMode_ = false;
    std::string brushPrefab_;
};

}  // namespace Elysium

#pragma once

#include <string>
#include "Core/Scene.h"
#include "Core/ServiceLocator.h"

namespace Elysium {

class EditorApplication;
namespace Services {
class ISceneService;
}

// A scene tab's settings, shown in the Viewport in place of the rendered scene: its
// properties, layers and systems, all editable live in one scrolling layout.
class SceneSettings {
public:
    explicit SceneSettings(EditorApplication& editor);

    void Draw(Scene& scene);

private:
    void DrawProperties(Services::ISceneService& service, Scene& scene);
    // The editing grid. Not part of SceneConfiguration: the engine has no use for it. The editor
    // keeps it per document and saves it in the scene's editor metadata.
    void DrawGrid();
    void DrawLayers(Scene& scene);
    void DrawSystems(Scene& scene);
    void DrawSystemParameters(System& system);

    EditorApplication& editor_;
    ServiceLocator& services_;
    std::string zIndexError_;
    // The Name field's text while it's being typed; committed (renaming the file) on Enter.
    char nameBuffer_[128] = {};
    const Scene* nameBufferScene_ = nullptr;
};

}  // namespace Elysium

#pragma once

#include <string>
#include "Core/Editor.h"
#include "Core/Scene.h"

namespace Elysium {

namespace Services {
class ISceneService;
}

// The scene picked in the Scenes panel (or the top of the stack): its properties, layers
// and systems, all editable live in one scrolling layout.
class SceneEditor : public Editor {
public:
    static constexpr const char* Title = "Scene";

    explicit SceneEditor(ServiceLocator& services);

    void Draw() override;

private:
    void DrawProperties(Services::ISceneService& service, Scene& scene);
    void DrawLayers(Scene& scene);
    void DrawSystems(Scene& scene);
    void DrawSystemParameters(System& system);

    std::string zIndexError_;
};

}  // namespace Elysium

#pragma once

#include <string>
#include "Core/Scene.h"
#include "Core/ServiceLocator.h"

namespace Elysium {

namespace Services {
class ISceneService;
}

// A scene tab's settings, shown in the Viewport in place of the rendered scene: its
// properties, layers and systems, all editable live in one scrolling layout.
class SceneSettings {
public:
    explicit SceneSettings(ServiceLocator& services) : services_(services) {}

    void Draw(Scene& scene);

private:
    void DrawProperties(Services::ISceneService& service, Scene& scene);
    void DrawLayers(Scene& scene);
    void DrawSystems(Scene& scene);
    void DrawSystemParameters(System& system);

    ServiceLocator& services_;
    std::string zIndexError_;
};

}  // namespace Elysium

#pragma once

namespace Elysium {

namespace Services {
    class SceneService;
}

class SceneInspector {
public:
    SceneInspector() = default;
    ~SceneInspector() = default;

    void DrawUI(Services::SceneService& service);

private:
    void DrawScenesPanel(Services::SceneService& service);
    void DrawCurrentScenePanel(Services::SceneService& service);
    void DrawSystemsDrawer(Services::SceneService& service);
    void DrawAssetsDrawer(Services::SceneService& service);

    // Panel state
    float leftPanelWidth_ = 300.0f;
    bool isDraggingSplitter_ = false;
    int selectedSceneIndex_ = -1;
};

} // namespace Elysium

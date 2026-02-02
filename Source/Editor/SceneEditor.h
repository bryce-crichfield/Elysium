#pragma once

#include "Core/Editor.h"

namespace Elysium {

namespace Services {
class SceneService;
}

class SceneEditor : public Editor {
public:
    SceneEditor();

    void Draw(Application& app) override;

private:
    void DrawScenesPanel(Services::SceneService& service);
    void DrawCurrentScenePanel(Services::SceneService& service);
    void DrawSystemsDrawer(Services::SceneService& service);

    // Panel state
    float leftPanelWidth_ = 300.0f;
    int selectedSceneIndex_ = -1;
};

}  // namespace Elysium

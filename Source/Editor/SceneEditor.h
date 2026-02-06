#pragma once

#include <string>
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
    void DrawScenesTab(Services::SceneService& service);
    void DrawSceneTab(Services::SceneService& service);
    void DrawSystemsTab(Services::SceneService& service);

    // Panel state
    float leftPanelWidth_ = 300.0f;
    int selectedSceneIndex_ = -1;
    std::string zIndexError_;
};

}  // namespace Elysium

#pragma once

#include <string>
#include "Core/Editor.h"
#include "Core/Scene.h"

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

    // Returns the editor-selected scene if it is still in the stack, otherwise the top scene.
    Scene* GetEditorScene(Services::SceneService& service);

    // Panel state
    float leftPanelWidth_ = 300.0f;
    int selectedSceneIndex_ = -1;
    std::string zIndexError_;

    // The scene the editor is inspecting (independent of the active/top scene).
    Scene* editorSelectedScene_ = nullptr;
};

}  // namespace Elysium

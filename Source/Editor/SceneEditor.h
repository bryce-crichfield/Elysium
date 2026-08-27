#pragma once

#include <string>
#include "Core/Editor.h"
#include "Core/Scene.h"

namespace Elysium {

namespace Services {
class ISceneService;
}

class SceneEditor : public Editor {
public:
    explicit SceneEditor(ServiceLocator& services);

    void Draw() override;

private:
    void DrawScenesTab(Services::ISceneService& service);
    void DrawSceneTab(Services::ISceneService& service);
    void DrawSystemsTab(Services::ISceneService& service);

    // Returns the editor-selected scene if it is still in the stack, otherwise the top scene.
    Scene* GetEditorScene(Services::ISceneService& service);

    // Panel state
    float leftPanelWidth_ = 300.0f;
    int selectedSceneIndex_ = -1;
    std::string zIndexError_;

    // The scene the editor is inspecting (independent of the active/top scene).
    Scene* editorSelectedScene_ = nullptr;
};

}  // namespace Elysium

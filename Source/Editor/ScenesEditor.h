#pragma once

#include <string>
#include "Core/Editor.h"

namespace Elysium {

namespace Services {
class ISceneService;
class IEditorService;
}  // namespace Services

// Every scene the project registers, and the live scene stack. Picking a stack entry
// points the Scene panel at it.
class ScenesEditor : public Editor {
   public:
    static constexpr const char* Title = "Scenes";

    explicit ScenesEditor(ServiceLocator& services);

    void Draw() override;

   private:
    void DrawToolbar(Services::ISceneService& scenes);
    void DrawAvailable(Services::ISceneService& scenes);
    void DrawStack(Services::ISceneService& scenes, Services::IEditorService& editor);

    std::string selectedSceneName_;  // registry entry the Push/Replace buttons act on
    char search_[64] = "";
};

}  // namespace Elysium

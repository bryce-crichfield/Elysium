#pragma once

#include <string>
#include "Core/Editor.h"

namespace Elysium {

namespace Services {
class ISceneService;
class IEditorService;
}  // namespace Services

// Every scene the project registers. Opening one gives it a viewport tab holding the
// editor's own copy, loaded from disk; the game's scene stack is only used in Play mode.
class ScenesEditor : public Editor {
   public:
    static constexpr const char* Title = "Scenes";

    explicit ScenesEditor(ServiceLocator& services);

    void Draw() override;

   private:
    void DrawToolbar(Services::ISceneService& scenes, Services::IEditorService& editor);
    void DrawAvailable(Services::ISceneService& scenes, Services::IEditorService& editor);

    std::string selectedSceneName_;  // registry entry the Open button acts on
    char search_[64] = "";
};

}  // namespace Elysium

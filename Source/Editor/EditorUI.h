#pragma once

#include <memory>
#include <vector>

#include "Core/Application.h"
#include "Editor/Editor.h"

namespace Elysium {

// The in-engine editor's UI, owned by EditorApplication: ImGui and the panels, and the menu
// bar and dock layout in Editor mode.
class EditorUI {
   public:
    explicit EditorUI(EditorApplication& editor);

    void Initialize(const ApplicationConfig& config);
    void Draw(AppMode mode);
    void OnModeChanged(AppMode mode);
    void Shutdown();

   private:
    template <typename T>
    T* GetEditor() {
        for (auto& editor : editors_) {
            if (auto* typed = dynamic_cast<T*>(editor.get())) return typed;
        }
        return nullptr;
    }

    void DrawMenuBar(AppMode mode);
    void BuildDockLayout();
    void ReloadFonts();

    EditorApplication& editor_;
    ApplicationConfig config_;
    std::vector<std::unique_ptr<Editor>> editors_;

    bool pendingFontReload_ = false;
    bool editorLayoutBuilt_ = false;
    float editorLayoutWidth_ = 0.0f;   // viewport size the layout was built for; rebuilt on change
    float editorLayoutHeight_ = 0.0f;
    bool focusDefaultTabs_ = false;    // select the default tabs once the panels exist
};

}  // namespace Elysium

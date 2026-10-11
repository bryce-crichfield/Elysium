#pragma once

#include <optional>
#include <string>

#include "Editor/Widgets/OsFileDialog.h"

namespace Elysium {

// File > Open Project: a Project.xml, typed or picked with the OS file browser.
class ProjectDialog {
   public:
    void Open();

    // Draws the dialog while it's open. Returns the chosen Project.xml on the frame it's confirmed.
    std::optional<std::string> Draw();

   private:
    OsOpenFileDialog browser_;
    char path_[512] = "";
    std::string failedPath_;  // last path that didn't load, so it isn't retried every frame
    bool pendingOpen_ = false;
};

}  // namespace Elysium

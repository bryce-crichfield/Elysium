#pragma once

#include <optional>
#include <string>

#include "Core/AssetKind.h"

namespace Elysium {

// The one dialog for naming a new asset file: File > New, File > Save As, and the Assets
// panel's Create. The folder follows the project convention (a kind's files live in its
// folder), so all it asks for is a name, and for New without a fixed kind, the kind.
class AssetFileDialog {
   public:
    enum class Mode { New, SaveAs };
    struct Result {
        Mode mode;
        AssetKind kind;
        std::string fullPath;
    };

    // A new asset. With `kind` the kind is fixed; without, it's picked in the dialog (only
    // kinds the editor can create). `folder` (project-relative, inside the kind's folder) is
    // where it goes; empty means the kind's own folder.
    void OpenNew(std::optional<AssetKind> kind = std::nullopt, const std::string& folder = "");
    // A copy of the open asset at `fullPath`: same kind and folder, a new name.
    void OpenSaveAs(AssetKind kind, const std::string& fullPath);

    // Draws the dialog while it's open. Returns the choice on the frame it's confirmed.
    std::optional<Result> Draw();

   private:
    void SetKind(AssetKind kind);

    Mode mode_ = Mode::New;
    bool kindFixed_ = false;
    bool folderFixed_ = false;
    AssetKind kind_ = AssetKind::Prefab;
    std::string folder_;     // project-relative, no trailing slash
    std::string extension_;  // with the dot
    char name_[128] = "";
    bool pendingOpen_ = false;
};

}  // namespace Elysium

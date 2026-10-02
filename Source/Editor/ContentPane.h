#pragma once

#include <memory>
#include <string>

#include "Core/ServiceLocator.h"

namespace Elysium {

namespace Services {
struct EditorDocument;
}

// What the Viewport shows for an open asset that has no world (scripts, shaders, textures,
// sprites): one per open tab, created when the tab first shows and kept until it closes.
// Scenes and prefabs are drawn by the Viewport itself.
class ContentPane {
   public:
    virtual ~ContentPane() = default;
    // Controls on the Viewport's toolbar band, after its Save button.
    virtual void DrawToolbar() {}
    // The rest of the Viewport below the toolbar.
    virtual void Draw() = 0;
    // Writes the edited asset back to its file. False if there's nothing to save or it fails.
    virtual bool Save() { return false; }
    virtual bool CanSave() const { return false; }
    // Writes the asset, as edited, to a new file at `fullPath` (Save As). The original file
    // is left as it was saved. False if it fails.
    virtual bool SaveAs(const std::string& fullPath) { return false; }
};

// The pane for `document`'s kind, or null if it has none.
std::unique_ptr<ContentPane> MakeContentPane(ServiceLocator& services, const Services::EditorDocument& document);

// Each kind's pane, in its own file.
std::unique_ptr<ContentPane> MakeCodePane(ServiceLocator& services, const Services::EditorDocument& document);
std::unique_ptr<ContentPane> MakeTexturePane(ServiceLocator& services, const Services::EditorDocument& document);
std::unique_ptr<ContentPane> MakeSpritePane(ServiceLocator& services, const Services::EditorDocument& document);
std::unique_ptr<ContentPane> MakeModelPane(ServiceLocator& services, const Services::EditorDocument& document);

}  // namespace Elysium

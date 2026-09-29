#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Core/Editor.h"
#include "Core/Entity.h"

namespace Elysium::Services {
class IEditorService;
struct ComponentPlaceholder;
}

namespace Elysium {

// Edits the primary selected entity: its name, then one collapsible section per component
// (drawn by the component's own Inspect), then an Add Component picker.
class InspectorEditor : public Editor {
   public:
    static constexpr const char* Title = "Inspector";

    explicit InspectorEditor(ServiceLocator& services);

    void Draw() override;

   private:
    void DrawHeader(Services::IEditorService& service, Entity entity);
    void DrawComponent(Services::IEditorService& service, Entity entity, const Services::ComponentPlaceholder& placeholder, bool removable = true);
    void DrawAddComponent(Services::IEditorService& service, Entity entity);

    // The Inspector shows a single "primary" entity — the most recently selected one.
    // Multi-select is tracked by EditorService but has no dedicated UI yet.
    static Entity GetPrimarySelection(Services::IEditorService& service);

    // --- Undo recording -----------------------------------------------------------------
    // Component UI writes straight into the live component — every component's Inspect does,
    // and the eleven hand-written ones do it with raw ImGui calls that no per-field hook could
    // intercept. So rather than trying to catch the write, the panel serializes each component
    // either side of drawing it and records the difference. One mechanism, every component.

    // A change seen this frame, queued rather than executed inline: whether it belongs in a
    // gesture can only be decided once the whole panel is drawn and ImGui knows if a widget is
    // still being held.
    struct PendingEdit {
        Entity entity = INVALID_ENTITY;
        std::string component;  // XML tag
        std::string before;
        std::string after;
        std::string label;
    };

    // Diffs `component` (an XML tag) on `entity` across `draw`, queueing an edit if it changed.
    void DrawDiffed(Services::IEditorService& service, Entity entity, const std::string& componentName,
                    const std::string& label, const std::function<void()>& draw);
    // Executes what the frame collected, opening or closing the drag gesture around it.
    void FlushEdits(Services::IEditorService& service);

    std::vector<PendingEdit> pendingEdits_;
    // Held open while a widget in this panel is, so one drag is one undo step.
    bool gestureOpen_ = false;

    // Name field buffer, refilled whenever the inspected entity changes.
    char nameBuffer_[256] = "";
    Entity nameBufferEntity_ = INVALID_ENTITY;

    char componentSearch_[64] = "";

    // Component removal is deferred until after the component loop to avoid iterator invalidation.
    std::string componentToRemove_;
    std::optional<bool> openRequest_;  // expand/collapse all, applied for one frame
};

}  // namespace Elysium

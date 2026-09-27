#pragma once

#include <optional>
#include <string>
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
    // When a prefab document is active: the prefab's exposed parameters, above the entity.
    void DrawPrefabParameters(Services::IEditorService& service, Entity selected);
    void DrawHeader(Services::IEditorService& service, Entity entity);
    void DrawComponent(Services::IEditorService& service, Entity entity, const Services::ComponentPlaceholder& placeholder);
    void DrawAddComponent(Services::IEditorService& service, Entity entity);

    // The Inspector shows a single "primary" entity — the most recently selected one.
    // Multi-select is tracked by EditorService but has no dedicated UI yet.
    static Entity GetPrimarySelection(Services::IEditorService& service);

    // Name field buffer, refilled whenever the inspected entity changes.
    char nameBuffer_[256] = "";
    Entity nameBufferEntity_ = INVALID_ENTITY;

    char componentSearch_[64] = "";

    // Component removal is deferred until after the component loop to avoid iterator invalidation.
    std::string componentToRemove_;
    std::optional<bool> openRequest_;  // expand/collapse all, applied for one frame
};

}  // namespace Elysium

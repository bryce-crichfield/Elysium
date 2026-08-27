#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Core/Entity.h"
#include "Interfaces/IEditorService.h"
#include "Service.h"

namespace Elysium {
class World;
}  // namespace Elysium

namespace Elysium::Services {

class EditorService : public Elysium::Service, public IEditorService {
   public:
    EditorService(ServiceLocator& registry);
    ~EditorService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    Elysium::World* GetWorld() const override;

    // Component introspection (used by WorldEditor)
    const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const override { return componentPlaceholders; }

    // Selection — shared between WorldEditor's entity list/inspector and viewport picking.
    const std::vector<Entity>& GetSelectedEntities() const override { return selectedEntities_; }
    void SelectEntity(Entity entity, bool additive = false) override;
    void ClearSelection() override;
    bool IsSelected(Entity entity) const override;

    // The free camera RenderSystem renders through while in AppMode::Editor.
    EditorCamera& GetEditorCamera() override { return editorCamera_; }

   private:
    std::vector<ComponentPlaceholder> componentPlaceholders;
    std::vector<Entity> selectedEntities_;
    EditorCamera editorCamera_;

    void RegisterComponentTypes();
};

}  // namespace Elysium::Services

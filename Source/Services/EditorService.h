#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Core/Entity.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/IEditorService.h"

namespace Elysium {
class World;
}  // namespace Elysium

namespace Elysium::Services {

class EditorService : public IEditorService {
   public:
    EditorService(ServiceLocator& registry);
    ~EditorService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    Elysium::World* GetWorld() const override;

    // Component introspection (used by InspectorEditor)
    const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const override { return componentPlaceholders; }

    // Selection — shared between the Hierarchy, Inspector and Viewport panels.
    const std::vector<Entity>& GetSelectedEntities() const override { return selectedEntities_; }
    void SelectEntity(Entity entity, bool additive = false) override;
    void ClearSelection() override;
    bool IsSelected(Entity entity) const override;

    // The free camera RenderSystem renders through while in AppMode::Editor.
    EditorCamera& GetEditorCamera() override { return editorCamera_; }

    Elysium::Scene* GetInspectedScene() override;
    void SetInspectedScene(Elysium::Scene* scene) override { inspectedScene_ = scene; }

   private:
    ServiceLocator& registry_;

    std::vector<ComponentPlaceholder> componentPlaceholders;
    std::vector<Entity> selectedEntities_;
    EditorCamera editorCamera_;
    Elysium::Scene* inspectedScene_ = nullptr;  // may dangle once popped; validated on read

    void RegisterComponentTypes();
};

}  // namespace Elysium::Services

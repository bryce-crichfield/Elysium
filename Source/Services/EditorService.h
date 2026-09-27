#pragma once

#include <functional>
#include <memory>
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

    Entity DuplicateEntity(Entity entity) override;
    void DeleteEntity(Entity entity) override;
    Entity CreateEntity(Entity parent = INVALID_ENTITY) override;
    bool CanBeRoot(Entity entity) const override;


    Elysium::Scene* GetViewportScene() override;

    void OpenScene(const std::string& sceneName) override;
    void OpenPrefab(const std::string& fullPath) override;
    const std::vector<std::unique_ptr<EditorDocument>>& GetDocuments() const override { return documents_; }
    int GetActiveDocument() const override { return activeDocument_; }
    void SetActiveDocument(int index) override;
    void CloseDocument(int index) override;
    void OpenAsset(const std::string& fullPath) override;
    bool SaveActiveDocument() override;
    Entity InstantiatePrefab(const std::string& fullPath) override;
    bool CreateAsset(AssetKind kind, const std::string& fullPath) override;
    bool SaveActiveDocumentAs(const std::string& fullPath) override;
    void ReplaceActiveDocument(const std::string& fullPath) override;
    bool CreatePrefabFromEntity(Entity entity, const std::string& fullPath) override;

   private:
    ServiceLocator& registry_;

    std::vector<ComponentPlaceholder> componentPlaceholders;
    std::vector<Entity> selectedEntities_;
    EditorCamera editorCamera_;
    bool openedEntryScene_ = false;
    // Layers/systems for prefab documents when no scene document is open (the entry scene).
    std::shared_ptr<Elysium::Scene> fallbackHost_;

    std::vector<std::unique_ptr<EditorDocument>> documents_;
    int activeDocument_ = -1;
    EditorDocument* ActiveDocument() const;
    int FindDocument(const std::string& fullPath) const;
    void AddDocument(std::unique_ptr<EditorDocument> doc);
    // The scene prefab documents take their layers and systems from.
    const Elysium::Scene* HostScene();
    // The active prefab document's root entity, if it has one.
    Entity PrefabRoot() const;
    bool SavePrefabDocument(EditorDocument& doc);
    Entity DuplicateSubtree(Elysium::World& world, Entity entity, Entity newParent);
    // Directory of the file the active tab saves to, for relative prefab paths.
    std::string ActiveOwnerDir() const;

    void RegisterComponentTypes();
};

}  // namespace Elysium::Services

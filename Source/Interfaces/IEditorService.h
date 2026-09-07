#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include "Interfaces/IService.h"

namespace Elysium {
class World;
}  // namespace Elysium

namespace Elysium::Services {

struct ComponentPlaceholder {
    std::function<void(Entity, Elysium::World*)> drawFunc;
    std::function<bool(Entity, Elysium::World*)> hasComponentFunc;
    std::function<void(Entity, Elysium::World*)> addComponentFunc;
    std::function<void(Entity, Elysium::World*)> removeComponentFunc;
    std::function<void(Entity, Elysium::World*)> resetComponentFunc;
    std::string name;
};

// The editor's free/independent camera — decoupled from any in-scene CameraComponent so
// the viewport can be panned/zoomed around the scene without touching game state.
struct EditorCamera {
    Vector2 position = {0, 0};
    float zoom = 1.0f;
    bool initialized = false;
};

class IEditorService : public IService {
   public:
    virtual Elysium::World* GetWorld() const = 0;

    virtual const std::vector<ComponentPlaceholder>& GetComponentPlaceholders() const = 0;

    virtual const std::vector<Entity>& GetSelectedEntities() const = 0;
    virtual void SelectEntity(Entity entity, bool additive = false) = 0;
    virtual void ClearSelection() = 0;
    virtual bool IsSelected(Entity entity) const = 0;

    virtual EditorCamera& GetEditorCamera() = 0;
};

}  // namespace Elysium::Services

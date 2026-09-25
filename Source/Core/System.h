#pragma once
#include <map>
#include <string>

#include "Core/Event.h"
#include "Core/Message.h"
#include "Core/Value.h"

namespace Elysium {
class ServiceLocator;
class Scene;
class World;
}  // namespace Elysium

namespace Elysium {

// A system's tunables by name. The defaults a system declares also fix each one's type:
// <System type="RenderSystem" ySort="false" /> attributes are parsed against them, and the
// Scene Editor's Systems tab picks a widget from them.
using SystemParameters = std::map<std::string, Value>;

struct Context {
    ServiceLocator* services;
    Scene* scene;
    World* world;
};

class System : public IEventListener, public IMessageListener {
protected:
    ServiceLocator* services;
    Scene* scene;
    World* world;

    bool isEnabled_ = true;
    bool isVisible_ = true;
    std::string name_;
    SystemParameters parameters_;

    // Override to declare parameters (name -> default).
    virtual SystemParameters DefaultParameters() const { return {}; }
    // Called after Initialize and after every SetParameter, to re-read cached values.
    virtual void OnParametersChanged() {}

public:
    System(Context context) : services(context.services), scene(context.scene), world(context.world) {
    }
    virtual ~System() = default;

    virtual void Update(float deltaTime) {}
    virtual void Draw() {}

    // Structural systems (e.g. TransformSystem) override this to keep running while the
    // scene is paused, so editor edits (like dragging an entity) are still reflected —
    // without resuming gameplay simulation (movement, scripts, physics, etc).
    virtual bool RunsWhenPaused() const { return false; }
    
    virtual void OnEvent(Event& event) override {}
    virtual void OnMessage(Message& message) override {}

    // Name is set by SystemRegistry::Create() from the registered key.
    // Returns empty string if the system was not created through the registry.
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    // Declared defaults, overlaid with each of `values` that names a declared parameter
    // and has its type (anything else is ignored). SceneLoader calls it once, right after
    // creating the system.
    void Initialize(const SystemParameters& values) {
        parameters_ = DefaultParameters();
        for (const auto& [name, value] : values) {
            auto it = parameters_.find(name);
            if (it != parameters_.end() && it->second.TypeName() == value.TypeName()) it->second = value;
        }
        OnParametersChanged();
    }

    const SystemParameters& GetParameters() const { return parameters_; }
    SystemParameters GetDefaultParameters() const { return DefaultParameters(); }

    void SetParameter(const std::string& name, const Value& value) {
        auto it = parameters_.find(name);
        if (it == parameters_.end() || it->second.TypeName() != value.TypeName()) return;
        it->second = value;
        OnParametersChanged();
    }

    template <typename T>
    T GetParameter(const std::string& name, T fallback) const {
        auto it = parameters_.find(name);
        return it != parameters_.end() && it->second.Is<T>() ? it->second.As<T>() : fallback;
    }

    Scene* GetScene() const { return scene; }

    bool IsEnabled() const { return isEnabled_; }
    void SetEnabled(bool enabled) { isEnabled_ = enabled; }

    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
};

}  // namespace Elysium

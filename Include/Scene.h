#pragma once

namespace Elysium {
class System;    // This tells the compiler that the class System exists.
class Timeline;  // Forward declaration for Timeline
}  // namespace Elysium

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Entity.h"

namespace Elysium {

// Constants
constexpr float TILE_WIDTH = 32.0f;
constexpr float TILE_HEIGHT = 32.0f;

struct Application;

class Scene {
   public:
    Scene();
    virtual ~Scene();

    // Hook methods - can be overridden by subclasses
    virtual void OnUpdate(float deltaTime);
    virtual void OnDraw(Rectangle screen);
    virtual void OnEvent(class Event& event);
    virtual void OnMessage(class Message& message);
    virtual void OnEnter() {}
    virtual void OnExit() {}

    // Entity and system access
    World* GetWorld() { return world_.get(); }
    const std::vector<std::unique_ptr<System>>& GetSystems() const { return systems_; }
    void AddSystem(std::unique_ptr<System> system);

    // Timeline management
    Timeline* CreateTimeline(const std::string& name);
    Timeline* GetTimeline(const std::string& name);
    void RemoveTimeline(const std::string& name);
    const std::vector<std::unique_ptr<Timeline>>& GetTimelines() const { return timelines_; }

    // Called during XML loading to create scene-specific systems
    virtual void CreateCustomSystems() {}

   protected:
    // Core scene components
    std::unique_ptr<World> world_;
    std::vector<std::unique_ptr<System>> systems_;
    std::vector<std::unique_ptr<Timeline>> timelines_;
};

// Scene factory function type - declared after Scene class is defined
using SceneFactory = std::function<Scene*()>;

bool LoadScene(Scene& scene, const std::string& path);
bool SaveScene(Scene& scene, const std::string& path);

}  // namespace Elysium

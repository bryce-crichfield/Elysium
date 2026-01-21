#pragma once

#include "Scene.h"
#include "raylib.h"

// Forward declarations
namespace Elysium {
class ServerNetworkSystem;
class ClientNetworkSystem;
}  // namespace Elysium

namespace Elysium::Scenes {

class ExploreScene : public Scene {
   public:
    ExploreScene();
    virtual ~ExploreScene() = default;

    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnEnter() override;
    void OnExit() override;
    void OnMessage(Message& message) override;

   private:
    void SetupNetworkSystems();

    // Cached pointers to network systems (owned by systems_ vector)
    ServerNetworkSystem* serverNetworkSystem_ = nullptr;
    ClientNetworkSystem* clientNetworkSystem_ = nullptr;
};

}  // namespace Elysium::Scenes

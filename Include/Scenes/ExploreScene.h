#pragma once

#include "../Scene.h"
#include "raylib.h"

namespace Elysium::Scenes {

class ExploreScene : public Scene {
public:
    ExploreScene();
    virtual ~ExploreScene() = default;

    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnEnter() override;
    void OnExit() override;

    std::vector<Asset> GetAssets() override;
};

} // namespace Elysium::Scenes

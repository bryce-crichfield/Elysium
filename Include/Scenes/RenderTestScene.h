#pragma once

#include "../Scene.h"
#include "raylib.h"

namespace Elysium::Scenes {

class RenderTestScene : public Scene {
public:
    RenderTestScene();
    virtual ~RenderTestScene() = default;

    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnDebugDraw() override;
    void OnEnter() override;
    void OnExit() override;

protected:
    void CreateCustomSystems() override;

private:
    void SetupCameras(Rectangle screen);
    void SetupLayers();
    void CreateWorldObjects();
    void CreateLightObjects();
    void CreateUIObjects();
    void HandleInput(float deltaTime);

    Entity mainCameraEntity_;
    Entity minimapCameraEntity_;
    Entity playerEntity_;
    
    Entity worldLayerEntity_;
    Entity lightLayerEntity_;
    Entity uiLayerEntity_;
    
    float cameraSpeed_;
};

} // namespace Elysium::Scenes
#pragma once

#include "raylib.h"
#include "Scene.h"

namespace Elysium::Scenes {

class BattleScene : public Scene {
public:
    BattleScene();
    virtual ~BattleScene() = default;
    void OnUpdate(float deltaTime) override;
    void OnDraw(Rectangle screen) override;
    void OnDebugDraw() override;
    void OnInput(const InputEvent& event) override;
    void OnEnter() override;
    void OnExit() override;
    
    std::vector<Asset> GetAssets() override;
    
private:
    // Asset storage
    Texture2D warriorTexture_;
    Texture2D swordTexture_;
    Texture2D shieldTexture_;
    Texture2D battlegroundTexture_;
    Sound battleMusic_;
    Sound swordClash_;
    Sound victoryFanfare_;
    
    bool assetsLoaded_{false};
};
};
#include "Scenes/MenuScene.h"
#include "Application.h"
#include "Scenes/GameScene.h"
#include "Scenes/BattleScene.h"
#include "Systems/AnimationSystem.h"
#include "imgui.h"
#include <memory>
#include <cmath>
#include "tinyxml2.h"

using namespace tinyxml2;

#define FOREACH(xmlName, varname, list) for (XMLElement* varname = list->FirstChildElement(xmlName); varname != nullptr; varname = varname->NextSiblingElement(xmlName))

namespace Elysium::Scenes {

BattleScene::BattleScene() : Scene("BattleScene") {
    // Base class constructor already creates world_ and systems_
}

void BattleScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering Battle Scene - Action Composition Demo");

    // Setup Hero (RedBall) with composable action sequences
    Entity hero;
    if (world_->GetEntityByName("RedBall", &hero)) {
        auto animation = AnimationComponent();
        
        try {
            Elysium::ActionParser parser;
            parser.SetParameter("startX", "100");
            parser.SetParameter("startY", "200");
            parser.SetParameter("pauseTime", "1.5");
            parser.SetParameter("bounceCount", "4");
            parser.RegisterCallback("completionCallback", []() {
                TraceLog(LOG_INFO, "Action Complete");
            });

            auto action = parser.LoadFromXML("./Assets/Animation/Animation.xml");
            animation.actionQueue.push(action);
            TraceLog(LOG_INFO, "Successfully loaded action from XML");
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Failed to load XML action: %s", e.what());
            // Fallback to a simple action
            animation.actionQueue.push(
                Fx::Sequence(
                    Fx::MoveTo(100, 200, 3.0f),
                    Fx::Wait(1.5f),
                    Fx::MoveBy(0, -20, 0.2f)
                )
            );
        }
        
        world_->AddComponent<AnimationComponent>(hero, animation);
    }

    // Setup Enemy (BlueBall) with different action composition
    // Entity enemy;
    // if (world_->GetEntityByName("BlueBall", &enemy)) {
        // auto animation = AnimationComponent();
    // }
}

void BattleScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting Menu Scene");
}

void BattleScene::OnInput(const InputEvent& event) {

}

void BattleScene::OnUpdate(float deltaTime) {
    Scene::OnUpdate(deltaTime);
}

void BattleScene::OnDraw(Rectangle screen) {
    Scene::OnDraw(screen);
}

void BattleScene::OnDebugDraw() {
}

std::vector<Asset> BattleScene::GetAssets() {
    return {
        Asset(AssetType::MUSIC, "music", "./Assets/sounds/music.mp3")
    };
}
} // namespace Elysium::Scenes

#include "Application.h"
#include "Scenes/MenuScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/BattleScene.h"
#include <memory>

int main()
{
    Elysium::Application &app = Elysium::Application::GetInstance();

    if (!app.Initialize())
    {
        return -1;
    }

    app.DefineScene("MenuScene", std::make_unique<Elysium::Scenes::MenuScene>);
    app.DefineScene("GameScene", std::make_unique<Elysium::Scenes::GameScene>);
    app.DefineScene("BattleScene", std::make_unique<Elysium::Scenes::BattleScene>);
    auto menuScene = std::make_unique<Elysium::Scenes::MenuScene>();
    app.SetScene(std::move(menuScene));

    app.Run();

    return 0;
}
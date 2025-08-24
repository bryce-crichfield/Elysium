#include "Application.h"
#include "Services/Services.h"
#include "Scenes/Scenes.h"

int main()
{
    Elysium::Application &app = Elysium::Application::GetInstance();

    if (!app.Initialize())
    {
        
        return -1;
    }

    auto &sceneService = app.GetService<Elysium::Services::SceneService>("SceneService");
    sceneService.RegisterScene("MenuScene", "", []() { return new Elysium::Scenes::MenuScene(); });
    sceneService.RegisterScene("OverworldScene", "", []() { return new Elysium::Scenes::OverworldScene(); });
    sceneService.RegisterScene("ExploreScene", "./Assets/ExploreScene.xml", []() { return new Elysium::Scenes::ExploreScene(); });
    sceneService.RegisterScene("BattleScene", "./Assets/Scene.xml", []() { return new Elysium::Scenes::BattleScene(); });
    sceneService.SetScene("MenuScene");

    app.Run();

    app.Shutdown();

    return 0;
}

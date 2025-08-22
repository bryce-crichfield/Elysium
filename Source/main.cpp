#include "Application.h"
#include "Scenes/MenuScene.h"
#include "Scenes/OverworldScene.h"
#include "Scenes/ExploreScene.h"
#include "Scenes/BattleScene.h"
#include <memory>

int main()
{
    Elysium::Application &app = Elysium::Application::GetInstance();

    if (!app.Initialize())
    {
        return -1;
    }

    app.DefineScene("MenuScene", "", []() { return new Elysium::Scenes::MenuScene(); });
    app.DefineScene("OverworldScene", "", []() { return new Elysium::Scenes::OverworldScene(); });
    app.DefineScene("ExploreScene", "./Assets/ExploreScene.xml", []() { return new Elysium::Scenes::ExploreScene(); });
    app.DefineScene("BattleScene", "./Assets/Scene.xml", []() { return new Elysium::Scenes::BattleScene(); });

    // Start with MemoryTestScene to test memory tracking
    app.SetScene("MenuScene");

    app.Run();

    app.Shutdown();

    return 0;
}

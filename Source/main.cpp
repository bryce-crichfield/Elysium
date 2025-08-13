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

    app.DefineScene("MenuScene", []() { return std::make_unique<Elysium::Scenes::MenuScene>(); });
    app.DefineScene("OverworldScene", []() { return std::make_unique<Elysium::Scenes::OverworldScene>(); });
    app.DefineScene("ExploreScene", []() { return std::make_unique<Elysium::Scenes::ExploreScene>(); });
    app.DefineScene("BattleScene", []() { return std::make_unique<Elysium::Scenes::BattleScene>(); });

    // Start with MemoryTestScene to test memory tracking
    auto menuScene = std::make_unique<Elysium::Scenes::MenuScene>();
    app.SetScene(std::move(menuScene));

    app.Run();

    return 0;
}

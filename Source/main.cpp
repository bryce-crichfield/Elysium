#include "Core/Application.h"
#include "Core/Common.h"
#include "Scenes/Scenes.h"
#include "Services/Services.h"

int main() {
    Profile;
    Elysium::Application& app = Elysium::Application::GetInstance();

    if (!app.Initialize()) {
        return -1;
    }

    auto& sceneService = app.GetService<Elysium::Services::SceneService>();
    sceneService.RegisterScene("MenuScene", "", []() { return new Elysium::Scenes::MenuScene(); });
    sceneService.RegisterScene("OverworldScene", "", []() { return new Elysium::Scenes::OverworldScene(); });
    sceneService.RegisterScene("ExploreScene", "ExploreScene.xml", []() { return new Elysium::Scenes::ExploreScene(); });
    sceneService.Push("MenuScene");

    app.Run();

    app.Shutdown();

    return 0;
}

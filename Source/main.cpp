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
    sceneService.RegisterScene("TestScene", "Scenes/TestScene.xml", []() { return new Elysium::Scene(); });
    sceneService.RegisterScene("MenuScene", "Scenes/MenuScene.xml", []() { return new Elysium::Scenes::MenuScene(); });
    sceneService.RegisterScene("ExploreScene", "Scenes/ExploreScene.xml", []() { return new Elysium::Scenes::ExploreScene(); });
    sceneService.Push("ExploreScene");

    app.Run();

    app.Shutdown();

    return 0;
}

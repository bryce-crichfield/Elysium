#include "Core/Application.h"
#include "Core/Common.h"
#include "Services/Services.h"

int main() {
    Profile;
    Elysium::Application& app = Elysium::Application::GetInstance();

    if (!app.Initialize()) {
        return -1;
    }

    auto& sceneService = app.GetService<Elysium::Services::SceneService>();
    sceneService.Push("ExploreScene");

    app.Run();

    app.Shutdown();

    return 0;
}

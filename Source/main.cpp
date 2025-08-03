#include "Application.h"
#include "Scenes/MenuScene.h"
#include <memory>

int main() {
    Elysium::Application& app = Elysium::Application::GetInstance();
    
    if (!app.Initialize()) {
        return -1;
    }
    
    auto menuScene = std::make_unique<Elysium::Scenes::MenuScene>(app.GetConfig());
    app.SetScene(std::move(menuScene));
    
    app.Run();
    
    return 0;
}
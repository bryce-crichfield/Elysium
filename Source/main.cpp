#include "Application.h"
#include "MenuScene.h"
#include <memory>

int main() {
    Application& app = Application::GetInstance();
    
    if (!app.Initialize()) {
        return -1;
    }
    
    auto menuScene = std::make_unique<MenuScene>();
    app.SetScene(std::move(menuScene));
    
    app.Run();
    app.Shutdown();
    
    return 0;
}
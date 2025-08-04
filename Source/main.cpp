#include "Application.h"
#include "Scenes/MenuScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/BattleScene.h"
#include <memory>

int main() {
    Elysium::Application& app = Elysium::Application::GetInstance();
    
    if (!app.Initialize()) {
        return -1;
    }
    
    // Register scene factories for XML loading
    app.DefineScene("MenuScene", [](const Elysium::GameConfig& config) {
        return std::make_unique<Elysium::Scenes::MenuScene>(config);
    });
    app.DefineScene("GameScene", [](const Elysium::GameConfig& config) {
        return std::make_unique<Elysium::Scenes::GameScene>(config);
    });
    app.DefineScene("BattleScene", [](const Elysium::GameConfig& config) {
        return std::make_unique<Elysium::Scenes::BattleScene>(config);
    });
    
    auto menuScene = std::make_unique<Elysium::Scenes::MenuScene>(app.GetConfig());
    app.SetScene(std::move(menuScene));
    
    app.Run();
    
    return 0;
}
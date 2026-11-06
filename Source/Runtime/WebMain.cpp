#include <emscripten.h>
#include <memory>
#include "Core/Common.h"
#include "Core/Path.h"
#include "Core/Project.h"
#include "Interfaces/ISceneService.h"
#include "Runtime/Application.h"
#include "Runtime/ApplicationService.h"
#include "Services/AssetService.h"
#include "Services/InvokeService.h"
#include "Services/MessageService.h"
#include "Services/SceneService.h"
#include "Services/ScriptService.h"
#include "Services/Web/WebAudioService.h"
#include "Services/Web/WebLogService.h"
#include "Services/Web/WebNetworkService.h"
#include "Services/Web/WebTaskService.h"

namespace {

using namespace Elysium;
using namespace Elysium::Services;

void RegisterServices(Application& app) {
    ServiceLocator& services = app.GetServiceLocator();
    services.Register<ApplicationService, IApplicationService>(std::make_unique<ApplicationService>(services, app));
    services.Register<WebLogService, ILogService>(std::make_unique<WebLogService>(services));
    services.Register<MessageService, IMessageService>(std::make_unique<MessageService>(services));
    services.Register<WebNetworkService, INetworkService>(std::make_unique<WebNetworkService>(services));
    services.Register<InvokeService, IInvokeService>(std::make_unique<InvokeService>(services));
    services.Register<WebTaskService, ITaskService>(std::make_unique<WebTaskService>(services));
    services.Register<AssetService, IAssetService>(std::make_unique<AssetService>(services));
    services.Register<WebAudioService, IAudioService>(std::make_unique<WebAudioService>(services));
    services.Register<SceneService, ISceneService>(std::make_unique<SceneService>(services));
    services.Register<ScriptService, IScriptService>(std::make_unique<ScriptService>(services));
}

// The browser owns the loop and calls back once per animation frame. main returns into it, so
// the app lives on the heap for the life of the page.
Application* g_app = nullptr;

void Frame() {
    if (g_app->Frame()) return;
    emscripten_cancel_main_loop();
    g_app->Shutdown();
}

}  // namespace

// The project is baked into the page's preloaded filesystem at build time (ELYSIUM_WEB_PROJECT).
int main() {
    ProjectConfig project;
    if (!ProjectConfig::FromXML(ELYSIUM_WEB_PROJECT, project)) {
        return -1;
    }
    Path::SetAssetsRoot(project.rootDir);

    g_app = new Application();
    RegisterServices(*g_app);
    if (!g_app->Initialize(project.configPath)) {
        return -1;
    }

    auto& sceneService = g_app->GetServiceLocator().Get<ISceneService>();
    sceneService.SetEntryScene(project.entryScene);
    sceneService.SetLoadingScene(project.loadingScene);
    sceneService.Push(project.entryScene);

    emscripten_set_main_loop(Frame, 0, true);
    return 0;
}

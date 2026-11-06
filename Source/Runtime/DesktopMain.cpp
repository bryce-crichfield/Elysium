#include "Core/CommandLine.h"
#include "Core/Common.h"
#include "Core/Path.h"
#include "Core/Project.h"
#include "Editor/EditorApplication.h"
#include "Interfaces/ISceneService.h"
#include "Runtime/Application.h"
#include "Runtime/ApplicationService.h"
#include "Services/Services.h"

namespace {

using namespace Elysium;
using namespace Elysium::Services;

void RegisterServices(Application& app) {
    ServiceLocator& services = app.GetServiceLocator();
    services.Register<ApplicationService, IApplicationService>(std::make_unique<ApplicationService>(services, app));
    services.Register<LogService, ILogService>(std::make_unique<LogService>(services));
    services.Register<MessageService, IMessageService>(std::make_unique<MessageService>(services));
    services.Register<NetworkService, INetworkService>(std::make_unique<NetworkService>(services));
    services.Register<InvokeService, IInvokeService>(std::make_unique<InvokeService>(services));
    services.Register<TaskService, ITaskService>(std::make_unique<TaskService>(services));
    services.Register<AssetService, IAssetService>(std::make_unique<AssetService>(services));
    services.Register<AudioService, IAudioService>(std::make_unique<AudioService>(services));
    services.Register<SceneService, ISceneService>(std::make_unique<SceneService>(services));
    services.Register<ScriptService, IScriptService>(std::make_unique<ScriptService>(services));
}

// One run of the app on one project; `next` is set when the app asked to switch projects.
// Each run gets a fresh Application, so nothing carries over between projects.
bool RunProject(const std::string& projectPath, bool editor, std::optional<std::string>& next) {
    Elysium::ProjectConfig project;
    if (!Elysium::ProjectConfig::FromXML(projectPath, project)) {
        return false;
    }
    Elysium::Path::SetAssetsRoot(project.rootDir);

    Elysium::Application app;
    RegisterServices(app);
    if (!app.Initialize(project.configPath)) {
        return false;
    }
    app.SetEditor(std::make_unique<EditorApplication>(app.GetServiceLocator()));

    if (editor) {
        app.SetMode(Elysium::AppMode::Editor);
    }

    auto& sceneService = app.GetServiceLocator().Get<Elysium::Services::ISceneService>();
    sceneService.SetEntryScene(project.entryScene);
    sceneService.SetLoadingScene(project.loadingScene);
    sceneService.Push(project.entryScene);

    next = app.Run();

    app.Shutdown();

    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Profile;

    Elysium::CommandLineArgs args = Elysium::CommandLine::Parse(argc, argv);

    std::string project = args.projectPath;
    bool editor = args.editor;
    while (true) {
        std::optional<std::string> next;
        if (!RunProject(project, editor, next)) {
            return -1;
        }
        if (!next) break;
        // Projects are only switched from the editor, so the next one opens in it too.
        project = *next;
        editor = true;
    }

    return 0;
}

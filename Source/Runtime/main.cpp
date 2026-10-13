#include "Runtime/Application.h"
#include "Core/CommandLine.h"
#include "Core/Common.h"
#include "Core/Path.h"
#include "Core/Project.h"
#include "Interfaces/ISceneService.h"
#include "Services/Services.h"

namespace {

// One run of the app on one project; `next` is set when the app asked to switch projects.
// Each run gets a fresh Application, so nothing carries over between projects.
bool RunProject(const std::string& projectPath, bool editor, std::optional<std::string>& next) {
    Elysium::ProjectConfig project;
    if (!Elysium::ProjectConfig::FromXML(projectPath, project)) {
        return false;
    }
    Elysium::Path::SetAssetsRoot(project.rootDir);

    Elysium::Application app;

    if (!app.Initialize(project.configPath)) {
        return false;
    }

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

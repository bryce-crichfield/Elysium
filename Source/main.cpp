#include "Core/Application.h"
#include "Core/CommandLine.h"
#include "Core/Common.h"
#include "Core/Path.h"
#include "Core/Project.h"
#include "Interfaces/ISceneService.h"
#include "Services/Services.h"

int main(int argc, char** argv) {
    Profile;

    Elysium::CommandLineArgs args = Elysium::CommandLine::Parse(argc, argv);

    Elysium::ProjectConfig project;
    if (!Elysium::ProjectConfig::FromXML(args.projectPath, project)) {
        return -1;
    }
    Elysium::Path::SetAssetsRoot(project.rootDir);

    Elysium::Application app;

    if (!app.Initialize(project.configPath)) {
        return -1;
    }

    if (args.editor) {
        app.SetMode(Elysium::AppMode::Editor);
    }

    auto& sceneService = app.GetServiceLocator().Get<Elysium::Services::ISceneService>();
    sceneService.SetEntryScene(project.entryScene);
    sceneService.Push(project.entryScene);

    app.Run();

    app.Shutdown();

    return 0;
}

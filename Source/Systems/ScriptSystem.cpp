#include "ScriptSystem.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Core/Component.h"
#include "Core/Application.h"
#include "Services/ScriptService.h"
#include "Components/ScriptComponent.h"

namespace Elysium::Systems {

ScriptSystem::ScriptSystem(Context context) : System(context) {}

void ScriptSystem::Update(float deltaTime) {
    auto& scriptService = Application::GetInstance().GetService<Services::ScriptService>();
    scriptService.SetActiveWorld(world);

    world->Query<ScriptComponent>([&](Entity entity, auto& scriptComp) {
        if (!scriptComp.isActive) return;

        for (size_t i = 0; i < scriptComp.scriptNames.size(); ++i) {
            if (scriptComp.scriptNames[i].empty()) continue;

            if (!scriptComp.isInitialized[i]) {
                scriptService.InitializeEntity(entity, Path(scriptComp.scriptNames[i]));
                scriptComp.isInitialized[i] = true;
            }
            scriptService.UpdateEntity(entity, Path(scriptComp.scriptNames[i]), deltaTime);
        }
    });
}

void ScriptSystem::OnEvent(Event& event) {
    // Skip input events in Editor mode
    if (Application::GetInstance().GetMode() == AppMode::Editor) {
        return;
    }

    auto& scriptService = Application::GetInstance().GetService<Services::ScriptService>();
    scriptService.SetActiveWorld(world);

    // For other events, dispatch to all scripted entities
    world->Query<ScriptComponent>([&](Entity entity, auto& scriptComp) {
        if (!scriptComp.isActive) return;

        for (size_t i = 0; i < scriptComp.scriptNames.size(); ++i) {
            if (scriptComp.scriptNames[i].empty()) continue;
            if (!scriptComp.isInitialized[i]) continue;

            scriptService.OnEntityEvent(entity, Path(scriptComp.scriptNames[i]), event);
        }
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::ScriptSystem)

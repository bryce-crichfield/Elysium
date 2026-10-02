#include "ScriptSystem.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Core/Component.h"
#include "Interfaces/IScriptService.h"
#include "Interfaces/ISceneService.h"
#include "Services/ScriptService.h"
#include "Services/SceneService.h"
#include "Core/Components/ScriptComponent.h"

namespace Elysium::Systems {

ScriptSystem::ScriptSystem(Context context) : System(context) {}

void ScriptSystem::Update(float deltaTime) {
    auto& scriptService = services->Get<Services::IScriptService>();
    scriptService.SetActiveWorld(world);

    world->Query<ScriptComponent>([&](Entity entity, auto& scriptComp) {
        if (!scriptComp.isActive) return;

        for (size_t i = 0; i < scriptComp.scriptNames.size(); ++i) {
            if (scriptComp.scriptNames[i].empty()) continue;

            if (!scriptComp.isInitialized[i]) {
                if (!scriptService.InitializeEntity(entity, Path(scriptComp.scriptNames[i]))) {
                    continue; // Asset not ready yet — retry next frame
                }
                scriptComp.isInitialized[i] = true;
            }
            scriptService.UpdateEntity(entity, Path(scriptComp.scriptNames[i]), deltaTime);
        }
    });
}

void ScriptSystem::OnEvent(Event& event) {
    // Skip input events while the simulation is paused (e.g. editing in Editor mode)
    if (!services->Get<Services::ISceneService>().IsPlaying()) {
        return;
    }

    auto& scriptService = services->Get<Services::IScriptService>();
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

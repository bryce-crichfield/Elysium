#include "ScriptSystem.h"
#include "Core/Component.h"
#include "Core/Application.h"
#include "Services/ScriptService.h"

namespace Elysium::Systems {

ScriptSystem::ScriptSystem(Context context) : System(context) {}

void ScriptSystem::Update(float deltaTime) {
    auto& scriptService = Application::GetInstance().GetService<Services::ScriptService>();

    world->Query<ScriptComponent>([&](Entity entity, auto& scriptComp) {
        if (scriptComp.isActive && !scriptComp.scriptName.empty()) {
            if (!scriptComp.isInitialized) {
                scriptService.InitEntity(entity, scriptComp.scriptName);
                scriptComp.isInitialized = true;
            }
            scriptService.UpdateEntity(entity, scriptComp.scriptName, deltaTime);
        }
    });
}

void ScriptSystem::OnEvent(Event& event) {
    auto& scriptService = Application::GetInstance().GetService<Services::ScriptService>();
    
    world->Query<ScriptComponent>([&](Entity entity, auto& scriptComp) {
        if (scriptComp.isActive && !scriptComp.scriptName.empty() && scriptComp.isInitialized) {
            scriptService.OnEntityEvent(entity, scriptComp.scriptName, event);
        }
    });
}

} // namespace Elysium::Systems

#include "ComponentRegistry.h"

namespace Elysium {
    ComponentRegistry& ComponentRegistry::Instance() {
        static ComponentRegistry instance;
        return instance;
    }

    void ComponentRegistry::RegisterAllComponents(World& world) {
        for (auto& reg : worldRegistrars_) {
            reg(world);
        }
    }

    void ComponentRegistry::BindAllScripts(sol::state& lua) {
        for (auto& binder : scriptBinders_) {
            binder(lua);
        }
    }

    const ComponentRegistry::LuaComponentAccess* ComponentRegistry::GetLuaAccess(const std::string& name) const {
        auto it = scriptAccessors_.find(name);
        if (it != scriptAccessors_.end()) {
            return &it->second;
        }
        return nullptr;
    }
}

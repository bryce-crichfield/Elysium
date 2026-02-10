#include "Core/SystemRegistry.h"
#include "Core/System.h"
#include "Services/LogService.h"

namespace Elysium {

SystemRegistry& SystemRegistry::Instance() {
    static SystemRegistry instance;
    return instance;
}

void SystemRegistry::Register(const std::string& name, SystemFactory factory) {
    factories_[name] = std::move(factory);
}

std::unique_ptr<System> SystemRegistry::Create(const std::string& name, Context context) const {
    auto it = factories_.find(name);
    if (it != factories_.end()) {
        return it->second(context);
    }
    LOG_WARNINGF("SystemRegistry", "Unknown system type: %s", name.c_str());
    return nullptr;
}

bool SystemRegistry::HasSystem(const std::string& name) const {
    return factories_.find(name) != factories_.end();
}

}  // namespace Elysium

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Elysium {

class System;
struct Context;

class SystemRegistry {
public:
    using SystemFactory = std::function<std::unique_ptr<System>(Context)>;

    static SystemRegistry& Instance();

    // Register a system factory by name
    void Register(const std::string& name, SystemFactory factory);

    // Create a system by name, returns nullptr if not found
    std::unique_ptr<System> Create(const std::string& name, Context context) const;

    // Check if a system is registered
    bool HasSystem(const std::string& name) const;

private:
    std::unordered_map<std::string, SystemFactory> factories_;
};

}  // namespace Elysium

// Helper macros for unique variable names
#define SYSTEM_REGISTER_CONCAT_IMPL(x, y) x##y
#define SYSTEM_REGISTER_CONCAT(x, y) SYSTEM_REGISTER_CONCAT_IMPL(x, y)

// Macro for easy system registration
// Usage: REGISTER_SYSTEM(Elysium::Systems::RenderSystem) in the .cpp file
// Registers using the short name (e.g., "RenderSystem") extracted from the full type
#define REGISTER_SYSTEM(FullType)                                               \
    static bool SYSTEM_REGISTER_CONCAT(_registered_system_, __COUNTER__) = [] { \
        std::string fullName = #FullType;                                       \
        size_t pos = fullName.rfind("::");                                      \
        std::string shortName = (pos != std::string::npos)                      \
            ? fullName.substr(pos + 2) : fullName;                              \
        ::Elysium::SystemRegistry::Instance().Register(                         \
            shortName,                                                          \
            [](::Elysium::Context ctx) -> std::unique_ptr<::Elysium::System> {  \
                return std::make_unique<FullType>(ctx);                         \
            });                                                                 \
        return true;                                                            \
    }();

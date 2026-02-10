#pragma once

#include <sol/forward.hpp>
#include "Core/Path.h"

namespace Elysium {
    struct Script {
        std::string source;
        Path path;
    };

    // Concept for components that can be bound to Lua
    template<typename T>
    concept Scriptable = requires(sol::usertype<T>& ut) {
        { T::BindLua(ut) } -> std::same_as<void>;
    };

    // Concept for components that can set values from Lua objects (for loading/initializing from Lua tables)
    template<typename T>
    concept LuaSettable = requires(T& c, sol::object v) {
        { T::SetFromLua(c, v) } -> std::same_as<void>;
    };

    }
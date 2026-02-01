#pragma once

#include <concepts>

#include "Core/Entity.h"
#include "Core/Xml.h"
#include "Core/Script.h"
#include "Core/Editor.h"

namespace Elysium {
    template<typename T>
    concept Component = requires {
        { T::Name() } -> std::convertible_to<const char*>;
    };
}

#pragma once
#include "Core/Component.h"
#include <string>
#include <vector>
#include "raylib.h"

namespace Elysium {
    struct LayerComponent {
        std::string name = "default";

        LayerComponent(const std::string& name = "default");

        static constexpr const char* Name() { return "Layer"; }
        static constexpr const char* XmlTag() { return "LayerComponent"; }

        static void LoadXml(LayerComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(LayerComponent& c, Entity e);
        static void BindLua(sol::usertype<LayerComponent>& ut);
        static void SetFromLua(LayerComponent& c, sol::object v);
    };
}

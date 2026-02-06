#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <vector>

namespace Elysium {
    struct CameraComponent {
        // Expects PositionComponent
        Rectangle viewport;
        float zoom = 1.0f;
        std::vector<int> layerMask;  // which layers this camera renders
        int renderOrder = 0;         // for multi-camera setups
        bool isVisible = true;

        CameraComponent();

        static constexpr const char* Name() { return "Camera"; }
        static constexpr const char* XmlTag() { return "CameraComponent"; }

        static void LoadXml(CameraComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(CameraComponent& c, Entity e);
        static void BindLua(sol::usertype<CameraComponent>& ut);
        static void SetFromLua(CameraComponent& c, sol::object v);
    };
}

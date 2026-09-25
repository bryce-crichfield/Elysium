#pragma once
#include "Core/Component.h"
#include "Core/MathTypes.h"

namespace Elysium {
    struct CameraComponent {
        // Expects TransformComponent
        Rectangle viewport;
        float zoom = 1.0f;
        int renderOrder = 0;         // for multi-camera setups
        bool isVisible = true;

        CameraComponent();

        static constexpr const char* Name() { return "Camera"; }
        static constexpr InspectorOrder Order = InspectorOrder::Rendering;
        static constexpr const char* XmlTag() { return "CameraComponent"; }

        static void LoadXml(CameraComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void Inspect(CameraComponent& c, Entity e, ServiceLocator& services);
        static void BindLua(sol::usertype<CameraComponent>& ut);
        static void SetFromLua(CameraComponent& c, sol::object v);
    };
}

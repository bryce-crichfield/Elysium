#pragma once
#include "Core/Component.h"
#include "Core/Math/MathTypes.h"

namespace Elysium {
    struct CameraComponent {
        // Expects TransformComponent
        Rectangle viewport;
        float zoom = 1.0f;
        // The orbit around the camera's position (World3D::View): degrees around the vertical,
        // and degrees down from the horizon. Yaw 0, pitch 30 is the iso picture.
        float yaw = 0.0f;
        float pitch = 30.0f;
        int renderOrder = 0;         // for multi-camera setups
        bool isVisible = true;

        CameraComponent();

        static constexpr const char* Name() { return "Camera"; }
        static constexpr const char* XmlTag() { return "CameraComponent"; }

        static void LoadXml(CameraComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static FieldList Fields();
        static void BindLua(sol::usertype<CameraComponent>& ut);
        static void SetFromLua(CameraComponent& c, sol::object v);
    };
}

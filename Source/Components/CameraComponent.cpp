#include "Components/CameraComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "Core/Component.h"
#include "Core/Xml.h"
#include "Interfaces/IApplicationService.h"
#include "imgui.h"

namespace Elysium {
    // Matches ApplicationConfig's own defaults (Core/Application.h) — a config-derived
    // viewport is applied in LoadXml once a ServiceLocator is available; this is just
    // the fallback for components default-constructed outside XML loading (e.g. Lua).
    CameraComponent::CameraComponent()
        : viewport{0, 0, 640.0f, 480.0f}, zoom(1.0f), renderOrder(0), isVisible(true) {
    }

    void CameraComponent::LoadXml(CameraComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        const auto& config = services.Get<Services::IApplicationService>().GetConfig();
        c.viewport = {0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight};

        c.zoom = el->FloatAttribute("zoom", 1.0f);
    }

    FieldList CameraComponent::Fields() {
        return {
            Field("Zoom", &CameraComponent::zoom, "zoom").Speed(0.01f).Range(0.1f, 10.0f),
            Field("Render Order", &CameraComponent::renderOrder),
            Field("Is Visible", &CameraComponent::isVisible),
        };
    }

    void CameraComponent::BindLua(sol::usertype<CameraComponent>& ut) {
        ut["viewport"] = &CameraComponent::viewport;
        ut["zoom"] = &CameraComponent::zoom;
        ut["renderOrder"] = &CameraComponent::renderOrder;
        ut["isVisible"] = &CameraComponent::isVisible;
    }

    void CameraComponent::SetFromLua(CameraComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.zoom = t.get_or("zoom", c.zoom);
            c.renderOrder = t.get_or("renderOrder", c.renderOrder);
            c.isVisible = t.get_or("isVisible", c.isVisible);
            // viewport?
        }
    }

    REGISTER_COMPONENT(CameraComponent);
}

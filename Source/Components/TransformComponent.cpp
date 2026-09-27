#include "Components/TransformComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "imgui.h"

namespace Elysium {
    TransformComponent::TransformComponent(float x, float y)
        : localX(x), localY(y), worldX(x), worldY(y) {}

    void TransformComponent::LoadXml(TransformComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.localX = el->FloatAttribute("x", 0.0f);
        c.localY = el->FloatAttribute("y", 0.0f);
        c.localScaleX = el->FloatAttribute("scaleX", 1.0f);
        c.localScaleY = el->FloatAttribute("scaleY", 1.0f);
        c.localRotation = el->FloatAttribute("rotation", 0.0f);

        // Seed the world cache so a same-frame read before TransformSystem's
        // first tick (e.g. immediately after load) sees a sane value.
        c.worldX = c.localX;
        c.worldY = c.localY;
        c.worldScaleX = c.localScaleX;
        c.worldScaleY = c.localScaleY;
        c.worldRotation = c.localRotation;
    }

    void TransformComponent::SaveXml(const TransformComponent& c, XMLBuilder& builder) {
        auto element = builder.AddElement("TransformComponent")
            .SetAttribute("x", c.localX)
            .SetAttribute("y", c.localY);

        if (c.localScaleX != 1.0f) element.SetAttribute("scaleX", c.localScaleX);
        if (c.localScaleY != 1.0f) element.SetAttribute("scaleY", c.localScaleY);
        if (c.localRotation != 0.0f) element.SetAttribute("rotation", c.localRotation);
    }

    FieldList TransformComponent::Fields() {
        return {
            Field("X", &TransformComponent::localX, "x").Section("Local"),
            Field("Y", &TransformComponent::localY, "y").Section("Local"),
            Field("Scale X", &TransformComponent::localScaleX, "scaleX").Speed(0.01f).Section("Local"),
            Field("Scale Y", &TransformComponent::localScaleY, "scaleY").Speed(0.01f).Section("Local"),
            Field("Rotation", &TransformComponent::localRotation, "rotation").Section("Local"),
            Field("X", &TransformComponent::worldX).Section("World"),
            Field("Y", &TransformComponent::worldY).Section("World"),
            Field("Scale X", &TransformComponent::worldScaleX).Section("World"),
            Field("Scale Y", &TransformComponent::worldScaleY).Section("World"),
            Field("Rotation", &TransformComponent::worldRotation).Section("World"),
        };
    }

    void TransformComponent::BindLua(sol::usertype<TransformComponent>& ut) {
        ut["localX"] = &TransformComponent::localX;
        ut["localY"] = &TransformComponent::localY;
        ut["localScaleX"] = &TransformComponent::localScaleX;
        ut["localScaleY"] = &TransformComponent::localScaleY;
        ut["localRotation"] = &TransformComponent::localRotation;

        ut["worldX"] = &TransformComponent::worldX;
        ut["worldY"] = &TransformComponent::worldY;
        ut["worldScaleX"] = &TransformComponent::worldScaleX;
        ut["worldScaleY"] = &TransformComponent::worldScaleY;
        ut["worldRotation"] = &TransformComponent::worldRotation;
        ut["worldDepth"] = &TransformComponent::worldDepth;
    }

    void TransformComponent::SetFromLua(TransformComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.localX = t.get_or("localX", c.localX);
            c.localY = t.get_or("localY", c.localY);
            c.localScaleX = t.get_or("localScaleX", c.localScaleX);
            c.localScaleY = t.get_or("localScaleY", c.localScaleY);
            c.localRotation = t.get_or("localRotation", c.localRotation);
        }
    }

    REGISTER_COMPONENT(TransformComponent);
}

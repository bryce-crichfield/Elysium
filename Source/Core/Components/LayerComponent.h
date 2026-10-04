#pragma once
#include "Core/Component.h"
#include <string>
#include <vector>

namespace Elysium {
    struct LayerComponent {
        std::string name = "default";
        bool isVisible = true;
        // Hidden while no vision light (LightComponent::vision) can see it: see VisibilitySystem.
        bool hideInFog = false;
        // In a standing World3D layer: lie on the ground at its own position (a decal under a
        // unit, depth-tested against the models) instead of standing as a card facing the camera.
        bool flat = false;
        // How opaque it draws, 0 to 1, times every ancestor's: fading a root fades all under it.
        float opacity = 1.0f;
        bool inFog = false;     // runtime: VisibilitySystem's verdict, never saved

        LayerComponent(const std::string& name = "default", bool isVisible = true);

        static constexpr const char* Name() { return "Layer"; }
        // Which layer a prefab placement sits on is a scene-composition decision, like where it
        // is — a painted wall goes on the layer being painted, not on whatever its prefab file
        // happens to declare. So it is editable and saved per placement.
        static constexpr bool PlacementOwned = true;
        static constexpr const char* XmlTag() { return "LayerComponent"; }

        static void LoadXml(LayerComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const LayerComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<LayerComponent>& ut);
        static void SetFromLua(LayerComponent& c, sol::object v);
    };
}

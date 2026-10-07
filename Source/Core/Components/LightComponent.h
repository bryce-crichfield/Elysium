#pragma once
#include "Core/Component.h"
#include "Core/Graphics.h"

namespace Elysium {
    // A point light in the 3D world (see RenderCompositor::Render3D). It stands `height` world
    // units above its entity's position, the way a sprite stands on its anchor, and casts
    // shadows from the models (not its own entity's: a unit carrying a light doesn't shadow it).
    // Lights every World3D layer at once. With `vision`, it's also a pair of eyes: what it can
    // see is clear of the fog of war (SceneLayer::fogOfWar). Vision with no intensity sees
    // without lighting.
    struct LightComponent {
        Color color = {255, 170, 90, 255};
        float intensity = 2.0f;
        float radius = 320.0f;   // world units the light reaches
        float height = 24.0f;    // above the entity's position
        float flicker = 0.0f;    // 0 steady .. 1 a guttering torch
        bool vision = false;

        static constexpr const char* Name() { return "Light"; }
        static constexpr const char* XmlTag() { return "LightComponent"; }

        static void LoadXml(LightComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const LightComponent& c, XMLBuilder& builder);
        static FieldList Fields();
    };
}

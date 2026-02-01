#pragma once
#include "Core/Component.h"
#include <vector>
#include <string>
#include "raylib.h"

namespace Elysium {
    struct LayerComponent {
        enum class Type {
            Background,
            World,
            Lighting,
            Overlay
        };

        enum class Space {
            World,
            Screen,
            Parallax
        };

        enum class Blend {
            Normal,
            Additive,
            Multiply,
            Alpha
        };

        int zIndex;
        Type type;
        Space space;
        Blend blend;

        float opacity = 1.0f;
        bool isVisible = true;

        std::string name;

        Vector2 parallaxFactor = {0.0f, 0.0f};  // 0 = no movement, 1 = full camera movement

        std::vector<Entity> allowedCameras;  // which camera entities can see this layer (empty = all)

        RenderTexture2D* framebuffer = nullptr;  // optional framebuffer to draw this layer to
        
        LayerComponent(int z = 0);

        static constexpr const char* Name() { return "Layer"; }
        static constexpr const char* XmlTag() { return "LayerComponent"; }

        static void LoadXml(LayerComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(LayerComponent& c, Entity e);
        static void BindLua(sol::usertype<LayerComponent>& ut);
        static void SetFromLua(LayerComponent& c, sol::object v);
    };
}

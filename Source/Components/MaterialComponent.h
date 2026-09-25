#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Component.h"
#include "Core/Value.h"

namespace Elysium {
    // One pass of a MaterialComponent: the entity's shape (its analytic SdfGeometry) is
    // drawn once more through the SDF shader composed from that geometry and `material`
    // (a chunk in Assets/Shaders/Sdf/Material). texturePath is bound as texture0 — only
    // meaningful for materials that sample it (Texture).
    struct MaterialLayer {
        std::string material = "Flat";
        bool enabled = true;
        std::string texturePath;
        std::unordered_map<std::string, Value> overrides;
    };

    // Draws the entity's shape as an ordered stack of analytic SDF layers — fill,
    // then stroke, then glow, drawn in list order (so put a glow first to sit behind).
    // Only applies to shapes that report SdfGeometry (Rectangle, Circle, Ellipse, Line, Polygon);
    // other renderables on the entity (Text, Tile, ...) draw as usual. A sprite is a
    // Rectangle + a Texture layer (SpriteSystem maintains both for SpriteComponent). padding grows
    // every layer's quad past the shape so glow / outside strokes have room.
    //
    // Shapes carry no style of their own — without a MaterialComponent they draw nothing.
    // A ShaderComponent on the same entity filters the result (see RenderShadedEntity).
    struct MaterialComponent {
        std::vector<MaterialLayer> layers;
        float padding = 0.0f;
        bool enabled = true;

        static constexpr const char* Name() { return "Material"; }
        static constexpr InspectorOrder Order = InspectorOrder::Material;
        static constexpr const char* XmlTag() { return "MaterialComponent"; }

        static void LoadXml(MaterialComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const MaterialComponent& c, XMLBuilder& builder);
        static void Inspect(MaterialComponent& c, Entity e, ServiceLocator& services);
        static void BindLua(sol::usertype<MaterialComponent>& ut);
    };
}

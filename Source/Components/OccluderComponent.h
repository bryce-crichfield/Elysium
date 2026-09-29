#pragma once
#include "Core/Component.h"
#include "Core/Graphics.h"
#include "Core/MathTypes.h"
#include <string>
#include <vector>

namespace Elysium {
    enum class OcclusionMode { None, FadeSelf, TintOccluded };

    const char* ToString(OcclusionMode mode);
    OcclusionMode ParseOcclusionMode(const std::string& text);

    // Footprint polygon (local, ground space) extruded `height` px upward: the visual volume
    // OcclusionSystem tests characters against. Empty footprint derives from the collider.
    struct OccluderComponent {
        std::string footprint;
        float height = 0.0f;
        bool isStatic = true;
        std::string mode = "FadeSelf";
        float fadeAlpha = 0.4f;
        Color tint = {90, 140, 255, 255};

        OcclusionMode Mode() const { return ParseOcclusionMode(mode); }
        std::vector<Vector2> LocalFootprint() const;

        static constexpr const char* Name() { return "Occluder"; }
        static constexpr InspectorOrder Order = InspectorOrder::Rendering;
        static constexpr const char* XmlTag() { return "OccluderComponent"; }

        static void LoadXml(OccluderComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const OccluderComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<OccluderComponent>& ut);
        static void SetFromLua(OccluderComponent& c, sol::object v);
    };
}

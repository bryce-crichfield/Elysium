#include "Components/OccluderComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Geometry.h"
#include "Core/Xml.h"

namespace Elysium {

    const char* ToString(OcclusionMode mode) {
        switch (mode) {
            case OcclusionMode::None:         return "None";
            case OcclusionMode::TintOccluded: return "TintOccluded";
            case OcclusionMode::FadeSelf:     return "FadeSelf";
        }
        return "FadeSelf";
    }

    OcclusionMode ParseOcclusionMode(const std::string& text) {
        if (text == "None")         return OcclusionMode::None;
        if (text == "TintOccluded") return OcclusionMode::TintOccluded;
        return OcclusionMode::FadeSelf;
    }

    std::vector<Vector2> OccluderComponent::LocalFootprint() const {
        auto polygon = ParsePointList(footprint);
        return polygon.size() >= 3 ? polygon : std::vector<Vector2>{};
    }

    void OccluderComponent::LoadXml(OccluderComponent& c, tinyxml2::XMLElement* el, ServiceLocator&) {
        c.footprint = ReadPolygonAttribute(el, "footprint");
        c.height    = el->FloatAttribute("height", c.height);
        c.isStatic  = el->BoolAttribute("isStatic", c.isStatic);
        c.fadeAlpha = el->FloatAttribute("fadeAlpha", c.fadeAlpha);
        if (const char* mode = el->Attribute("mode")) c.mode = mode;
        if (const char* tint = el->Attribute("tint")) c.tint = ParseHexColor(tint, c.tint);
    }

    void OccluderComponent::SaveXml(const OccluderComponent& c, XMLBuilder& builder) {
        builder.AddElement("OccluderComponent")
            .SetAttribute("footprint", c.footprint.c_str())
            .SetAttribute("height", c.height)
            .SetAttribute("isStatic", c.isStatic)
            .SetAttribute("mode", c.mode.c_str())
            .SetAttribute("fadeAlpha", c.fadeAlpha)
            .SetAttribute("tint", ColorToHex(c.tint).c_str());
    }

    FieldList OccluderComponent::Fields() {
        return {
            Field("Footprint", &OccluderComponent::footprint, "footprint"),
            Field("Height", &OccluderComponent::height, "height").Range(0.0f, 2000.0f),
            Field("Is Static", &OccluderComponent::isStatic, "isStatic"),
            Field("Mode", &OccluderComponent::mode, "mode").Section("Effect"),
            Field("Fade Alpha", &OccluderComponent::fadeAlpha, "fadeAlpha").Speed(0.01f).Range(0.0f, 1.0f),
            Field("Tint", &OccluderComponent::tint, "tint"),
        };
    }

    void OccluderComponent::BindLua(sol::usertype<OccluderComponent>& ut) {
        ut["footprint"] = &OccluderComponent::footprint;
        ut["height"]    = &OccluderComponent::height;
        ut["isStatic"]  = &OccluderComponent::isStatic;
        ut["mode"]      = &OccluderComponent::mode;
        ut["fadeAlpha"] = &OccluderComponent::fadeAlpha;
    }

    void OccluderComponent::SetFromLua(OccluderComponent& c, sol::object v) {
        if (!v.is<sol::table>()) return;
        sol::table t = v.as<sol::table>();
        if (t["footprint"].valid()) c.footprint = t["footprint"].get<std::string>();
        if (t["height"].valid())    c.height    = t["height"];
        if (t["isStatic"].valid())  c.isStatic  = t["isStatic"];
        if (t["mode"].valid())      c.mode      = t["mode"].get<std::string>();
        if (t["fadeAlpha"].valid()) c.fadeAlpha = t["fadeAlpha"];
    }

    REGISTER_COMPONENT(OccluderComponent);
}

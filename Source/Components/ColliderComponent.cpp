#include "Components/ColliderComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Geometry.h"
#include "Editor/Widgets.h"
#include "imgui.h"

namespace Elysium {
    const char* ToString(ColliderShape shape) {
        switch (shape) {
            case ColliderShape::Box:     return "Box";
            case ColliderShape::Circle:  return "Circle";
            case ColliderShape::Polygon: return "Polygon";
            case ColliderShape::Auto:    break;
        }
        return "Auto";
    }

    ColliderShape ParseColliderShape(const std::string& text) {
        if (text == "Box") return ColliderShape::Box;
        if (text == "Circle") return ColliderShape::Circle;
        if (text == "Polygon") return ColliderShape::Polygon;
        return ColliderShape::Auto;
    }

    ColliderShape ColliderComponent::ResolvedShape() const {
        ColliderShape declared = ParseColliderShape(shape);
        if (declared != ColliderShape::Auto) {
            // A declared Circle with no radius would be a zero-size collider; fall back.
            if (declared == ColliderShape::Circle && radius <= 0.0f) return ColliderShape::Box;
            if (declared == ColliderShape::Polygon && LocalPolygon().empty()) return ColliderShape::Box;
            return declared;
        }
        if (radius > 0.0f) return ColliderShape::Circle;
        if (!LocalPolygon().empty()) return ColliderShape::Polygon;
        return ColliderShape::Box;
    }

    std::vector<Vector2> ColliderComponent::LocalPolygon() const {
        auto polygon = ParsePointList(points);
        return polygon.size() >= 3 ? polygon : std::vector<Vector2>{};
    }

    std::vector<Vector2> ColliderComponent::GetPolygon(float posX, float posY) const {
        auto local = LocalPolygon();
        if (!local.empty()) return TranslatePolygon(local, {posX, posY});
        Rectangle r = GetRect(posX, posY);
        return {{r.x, r.y}, {r.x + r.width, r.y}, {r.x + r.width, r.y + r.height}, {r.x, r.y + r.height}};
    }

    void ColliderComponent::SyncBoxToPolygon() {
        auto local = LocalPolygon();
        if (local.empty()) return;
        Rectangle b = PolygonBounds(local);
        width = b.width;
        height = b.height;
        offsetX = b.x + b.width * 0.5f;
        offsetY = b.y + b.height * 0.5f;
    }

    void ColliderComponent::LoadXml(ColliderComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        if (el->Attribute("width")) c.width = el->FloatAttribute("width");
        if (el->Attribute("height")) c.height = el->FloatAttribute("height");
        if (el->Attribute("offsetX")) c.offsetX = el->FloatAttribute("offsetX");
        if (el->Attribute("offsetY")) c.offsetY = el->FloatAttribute("offsetY");
        if (el->Attribute("isTrigger")) c.isTrigger = el->BoolAttribute("isTrigger");
        if (el->Attribute("shape")) c.shape = el->Attribute("shape");
        if (el->Attribute("radius")) c.radius = el->FloatAttribute("radius");
        if (el->Attribute("bottom")) c.bottom = el->FloatAttribute("bottom");
        if (el->Attribute("top")) c.top = el->FloatAttribute("top");
        c.points = ReadPolygonAttribute(el, "points");
        c.SyncBoxToPolygon();
    }

    void ColliderComponent::SaveXml(const ColliderComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("ColliderComponent")
            .SetAttribute("width", c.width)
            .SetAttribute("height", c.height)
            .SetAttribute("offsetX", c.offsetX)
            .SetAttribute("offsetY", c.offsetY)
            .SetAttribute("isTrigger", c.isTrigger);
        if (!c.points.empty()) b.SetAttribute("points", c.points.c_str());
        if (c.shape != "Auto") b.SetAttribute("shape", c.shape.c_str());
        if (c.radius > 0.0f) b.SetAttribute("radius", c.radius);
        if (c.bottom != 0.0f) b.SetAttribute("bottom", c.bottom);
        if (c.top != 0.0f) b.SetAttribute("top", c.top);
    }

    FieldList ColliderComponent::Fields() {
        return {
            Field("Width", &ColliderComponent::width, "width").Range(0.0f, 1000.0f),
            Field("Height", &ColliderComponent::height, "height").Range(0.0f, 1000.0f),
            Field("Offset X", &ColliderComponent::offsetX, "offsetX"),
            Field("Offset Y", &ColliderComponent::offsetY, "offsetY"),
            Field("Is Trigger", &ColliderComponent::isTrigger, "isTrigger"),
            Field("Shape", &ColliderComponent::shape, "shape").Section("Shape"),
            Field("Radius", &ColliderComponent::radius, "radius").Speed(0.5f).Range(0.0f, 512.0f),
            Field("Polygon", &ColliderComponent::points, "points"),
            Field("Bottom", &ColliderComponent::bottom, "bottom").Section("Height").Speed(0.5f),
            Field("Top", &ColliderComponent::top, "top").Speed(0.5f),
        };
    }

    void ColliderComponent::BindLua(sol::usertype<ColliderComponent>& ut) {
        ut["width"] = &ColliderComponent::width;
        ut["height"] = &ColliderComponent::height;
        ut["offsetX"] = &ColliderComponent::offsetX;
        ut["offsetY"] = &ColliderComponent::offsetY;
        ut["isTrigger"] = &ColliderComponent::isTrigger;
        ut["shape"] = &ColliderComponent::shape;
        ut["radius"] = &ColliderComponent::radius;
        ut["bottom"] = &ColliderComponent::bottom;
        ut["top"] = &ColliderComponent::top;
        ut["points"] = sol::property(
            [](ColliderComponent& c) { return c.points; },
            [](ColliderComponent& c, const std::string& v) { c.points = v; c.SyncBoxToPolygon(); });
    }

    void ColliderComponent::SetFromLua(ColliderComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            if (t["width"].valid()) c.width = t["width"];
            if (t["height"].valid()) c.height = t["height"];
            if (t["offsetX"].valid()) c.offsetX = t["offsetX"];
            if (t["offsetY"].valid()) c.offsetY = t["offsetY"];
            if (t["isTrigger"].valid()) c.isTrigger = t["isTrigger"];
            if (t["shape"].valid()) c.shape = t["shape"].get<std::string>();
            if (t["radius"].valid()) c.radius = t["radius"];
            if (t["bottom"].valid()) c.bottom = t["bottom"];
            if (t["top"].valid()) c.top = t["top"];
            if (t["points"].valid()) { c.points = t["points"].get<std::string>(); c.SyncBoxToPolygon(); }
        }
    }

    REGISTER_COMPONENT(ColliderComponent);
}

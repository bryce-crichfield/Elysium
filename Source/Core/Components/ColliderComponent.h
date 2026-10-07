#pragma once
#include "Core/Component.h"
#include "Core/Math/Polygon.h"
#include "Core/Math/MathTypes.h"
#include <string>
#include <vector>

namespace Elysium {
    // What shape the narrow phase and the physics response actually use. Box keeps the
    // historical AABB behaviour; Circle is the right shape for an agent (a radial push-out
    // can't snap to an axis the way a box MTV does); Polygon honours `points` exactly, which
    // matters for a thin diagonal wall whose AABB is several times thicker than the wall.
    enum class ColliderShape { Auto, Box, Circle, Polygon };

    const char* ToString(ColliderShape shape);
    ColliderShape ParseColliderShape(const std::string& text);

    // AABB by default; an optional local `points` polygon gives NavigationSystem an exact carve
    // while width/height/offset stay derived from its bounds for the AABB paths.
    struct ColliderComponent {
        float width = 32.0f;
        float height = 32.0f;
        float offsetX = 0.0f;  // Offset from entity position
        float offsetY = 0.0f;
        bool isTrigger = false;  // Triggers don't cause physical response, just detection
        std::string points;
        // Auto resolves to Circle when radius > 0, else Polygon when points is set, else Box,
        // so existing scenes and prefabs keep their behaviour without being edited.
        std::string shape = "Auto";
        // Measured along x. In an isometric scene the reach along y is radius / isoRatio, so
        // the circle stays a circle on the ground instead of an ellipse twice as deep.
        float radius = 0.0f;
        // How high it stands, above its entity's z: from `bottom` to `top`. With top <= bottom
        // (the default) it has no height range and blocks at every height, as a 2D collider did.
        // Two colliders only touch when their ranges overlap, so a flier passes over a low wall,
        // and the navmesh only carves the floors it actually stands in the way on.
        float bottom = 0.0f;
        float top = 0.0f;

        ColliderComponent() = default;
        ColliderComponent(float w, float h, float ox = 0.0f, float oy = 0.0f)
            : width(w), height(h), offsetX(ox), offsetY(oy), isTrigger(false) {}

        // The heights it occupies with its entity at height `z`.
        void HeightRange(float z, float& low, float& high) const {
            if (top > bottom) { low = z + bottom; high = z + top; }
            else { low = -1e30f; high = 1e30f; }
        }

        // Get the world-space rectangle given entity position
        Rectangle GetRect(float posX, float posY) const {
            return Rectangle{
                posX + offsetX - width * 0.5f,
                posY + offsetY - height * 0.5f,
                width,
                height
            };
        }

        static constexpr const char* Name() { return "Collider"; }
        static constexpr const char* XmlTag() { return "ColliderComponent"; }

        Polygon LocalPolygon() const;
        Polygon GetPolygon(float posX, float posY) const;
        void SyncBoxToPolygon();

        // `shape` with Auto resolved against what this collider actually carries.
        ColliderShape ResolvedShape() const;
        // The circle/box centre in world space (the anchor plus the collider offset).
        Vector2 GetCenter(float posX, float posY) const { return {posX + offsetX, posY + offsetY}; }

        // A rect guaranteed to contain the resolved shape, for the broad phase. A circle's is
        // deliberately conservative along y (it ignores the isometric squash, which would need
        // isoRatio here) — an extra pair is free, a missed one is a tunnelling bug.
        Rectangle GetBroadRect(float posX, float posY) const {
            if (ResolvedShape() != ColliderShape::Circle) return GetRect(posX, posY);
            return Rectangle{posX + offsetX - radius, posY + offsetY - radius, radius * 2.0f, radius * 2.0f};
        }

        static void LoadXml(ColliderComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const ColliderComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<ColliderComponent>& ut);
        static void SetFromLua(ColliderComponent& c, sol::object v);
    };
}

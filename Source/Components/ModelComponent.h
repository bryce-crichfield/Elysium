#pragma once
#include "Core/Component.h"
#include "Core/Graphics.h"
#include <string>

namespace Elysium {
    // A 3D model standing at the entity's position, drawn by World3D layers (see
    // Core/World3D.h for how the 2D world maps into 3D). By default the model is placed by its
    // own bounds, not its file origin: its footprint centered on the position and its base on
    // the ground, so any model drops in without hand-tuned offsets.
    struct ModelComponent {
        std::string modelPath;
        float scale = 1.0f;          // world units per model unit
        float yaw = 0.0f;            // degrees, on top of the Transform's rotation
        Color tint = {255, 255, 255, 255};
        bool centered = true;        // place by bounds (footprint centered, base on the ground)
        bool walkable = false;       // units stand on it (GroundSystem): floors, stairs, platforms

        // Runtime: the loaded model, once the renderer has found it (for picking and bounds,
        // which only see the World).
        const Model* loaded = nullptr;

        static constexpr const char* Name() { return "Model"; }
        static constexpr InspectorOrder Order = InspectorOrder::Geometry;
        static constexpr const char* XmlTag() { return "ModelComponent"; }

        static void LoadXml(ModelComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const ModelComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<ModelComponent>& ut);
        static void SetFromLua(ModelComponent& c, sol::object v);
    };
}

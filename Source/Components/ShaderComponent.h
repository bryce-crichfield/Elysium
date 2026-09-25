#pragma once
#include <string>
#include <unordered_map>
#include "Core/Component.h"
#include "Core/Value.h"

namespace Elysium {
    class Shader;

    // Editor widget: one row per non-built-in uniform `shader` declares, showing the value
    // in effect (override or source default); editing creates an override, Reset drops it.
    // Shared by ShaderComponent and MaterialComponent's layers.
    void InspectUniformOverrides(const Shader& shader, std::unordered_map<std::string, Value>& overrides);

    // Diverts an entity's render records through a private offscreen buffer and a
    // compiled Shader asset (see RenderCompositor::RenderShadedEntity in
    // Systems/RenderSystem.cpp). shaderPath names a ShaderAsset (a .fs file, optionally
    // paired with a sibling .vs); padding grows the offscreen buffer past the entity's
    // bounds so a screen-space effect like glow/outline has room to bleed into.
    // overrides replaces a subset of the shader's declared uniforms with per-instance
    // values — anything not present here uses the default the shader source declares.
    struct ShaderComponent {
        std::string shaderPath;
        float padding = 0.0f;
        bool enabled = true;

        // The shaded box when ShaderComponent stands alone (it is then its own renderable,
        // see HasShaderImpl in Systems/RenderSystem.cpp). Also the fallback when the
        // entity's other renderables report no Bounds (e.g. Text/Line).
        float width = 100.0f;
        float height = 100.0f;

        std::unordered_map<std::string, Value> overrides;

        static constexpr const char* Name() { return "Shader"; }
        static constexpr const char* XmlTag() { return "ShaderComponent"; }

        static void LoadXml(ShaderComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const ShaderComponent& c, XMLBuilder& builder);
        static void Inspect(ShaderComponent& c, Entity e, ServiceLocator& services);
        static void BindLua(sol::usertype<ShaderComponent>& ut);
    };
}

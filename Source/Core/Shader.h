#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Core/Value.h"

namespace Elysium {

// A single `uniform <type> <name>;` reflected out of a shader's fragment source, plus
// the default value it carries in an optional trailing `// default: ...` comment (GLSL
// itself has no uniform initializers). isBuiltIn flags names the engine sets itself
// every draw — anything prefixed e_ (e_Time, e_Size, ... — see RenderCompositor) — so
// per-instance override logic skips them.
struct ShaderUniform {
    std::string name;
    std::string typeName;  // "bool" | "int" | "float" | "vec2" | "vec3" | "vec4"
    Value defaultValue;
    bool isBuiltIn = false;
};

// A compiled GLSL shader program plus its reflected uniform list. Move-only: owns a
// GPU program handle (backend: raylib) released by the destructor. Header stays
// raylib-free per the Core/RaylibConvert.h convention — see Shader.cpp.
class Shader {
   public:
    Shader();
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool IsValid() const;
    unsigned int Id() const;

    // Compiles vertexSource/fragmentSource and reflects fragmentSource's `uniform`
    // declarations into GetUniforms(). vertexSource may be empty — the backend's
    // default 2D vertex shader is used, which is what a post-process effect wants.
    // On failure returns an invalid Shader and, if `error` is non-null, fills it in.
    static Shader FromSource(const std::string& vertexSource, const std::string& fragmentSource,
                              std::string* error = nullptr);

    // No-op on an invalid shader or an unknown/unreflected uniform name.
    void SetAttribute(const std::string& name, const Value& value);

    // Uploads `count` elements of a float/vecN uniform array (components = 1..4). Arrays
    // aren't reflected (no per-instance overrides), so the location is looked up on first
    // use. No-op on an invalid shader or a name the compiler optimized out.
    void SetFloatArray(const std::string& name, const float* data, int count, int components);

    const std::vector<ShaderUniform>& GetUniforms() const { return uniforms_; }
    const std::vector<ShaderUniform>& GetAttributes() const { return uniforms_; }

    // Opaque pointer to the backend handle (a raylib ::Shader*), for RenderContext's
    // BeginShaderMode/EndShaderMode. Null for an invalid shader.
    const void* NativeHandle() const;

   private:
    struct Native;
    std::unique_ptr<Native> native_;
    std::vector<ShaderUniform> uniforms_;
};

}  // namespace Elysium

#include "Core/Shader.h"

#include <regex>
#include <unordered_map>
#include "Services/LogService.h"
#include "raylib.h"
#include "rlgl.h"

namespace Elysium {

namespace {

// Matches `uniform <type> <name>;` with an optional `// default: <text>` comment,
// since GLSL uniforms can't carry an initializer themselves.
const std::regex kUniformPattern(
    R"(uniform\s+(bool|int|float|vec2|vec3|vec4)\s+(\w+)\s*;(?:[^\S\n]*//[^\S\n]*default:?[^\S\n]*([^\n]*))?)");

bool IsBuiltInUniformName(const std::string& name) {
    // Anything prefixed e_ is engine-fed (e_Time, e_Size, ... — see RenderCompositor);
    // colDiffuse is raylib's own per-draw tint uniform, set automatically by the draw
    // call — none of these should be reflected as overridable per-instance uniforms, or
    // the override loop would stomp them with a zero default.
    return name.rfind("e_", 0) == 0 || name == "colDiffuse";
}

std::vector<ShaderUniform> ReflectUniforms(const std::string& fragmentSource) {
    std::vector<ShaderUniform> uniforms;
    auto begin = std::sregex_iterator(fragmentSource.begin(), fragmentSource.end(), kUniformPattern);
    for (auto it = begin; it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        ShaderUniform uniform;
        uniform.typeName = m[1].str();
        uniform.name = m[2].str();
        uniform.isBuiltIn = IsBuiltInUniformName(uniform.name);
        std::string defaultText = m[3].str();
        uniform.defaultValue = defaultText.empty() ? Value::FromString(uniform.typeName, "0")
                                                     : Value::FromString(uniform.typeName, defaultText);
        uniforms.push_back(std::move(uniform));
    }
    return uniforms;
}

}  // namespace

// Owns the GPU program: releasing a Native releases it, so Shader's moves are the defaults.
struct Shader::Native {
    ::Shader shader{};
    std::unordered_map<std::string, int> locations;
    ~Native() { if (shader.id != 0) ::UnloadShader(shader); }
};

Shader::Shader() = default;
Shader::~Shader() = default;
Shader::Shader(Shader&&) noexcept = default;
Shader& Shader::operator=(Shader&&) noexcept = default;

bool Shader::IsValid() const { return native_ && native_->shader.id != 0; }

unsigned int Shader::Id() const { return native_ ? native_->shader.id : 0; }

const void* Shader::NativeHandle() const {
    return (native_ && native_->shader.id != 0) ? &native_->shader : nullptr;
}

Shader Shader::FromSource(const std::string& vertexSource, const std::string& fragmentSource,
                           std::string* error) {
    const char* vs = vertexSource.empty() ? nullptr : vertexSource.c_str();
    ::Shader compiled = ::LoadShaderFromMemory(vs, fragmentSource.c_str());

    Shader result;
    if (compiled.id == 0) {
        if (error) *error = "compilation failed (see console log)";
        return result;
    }

    result.native_ = std::make_unique<Native>();
    result.native_->shader = compiled;
    result.uniforms_ = ReflectUniforms(fragmentSource);

    for (const ShaderUniform& uniform : result.uniforms_) {
        int location = ::GetShaderLocation(compiled, uniform.name.c_str());
        if (location != -1) {
            result.native_->locations[uniform.name] = location;
        }
    }

    return result;
}

void Shader::SetUniform(const std::string& name, const Value& value) {
    if (!native_) return;
    auto it = native_->locations.find(name);
    if (it == native_->locations.end()) return;
    int location = it->second;

    if (value.Is<bool>()) {
        int intValue = value.As<bool>() ? 1 : 0;
        ::SetShaderValue(native_->shader, location, &intValue, SHADER_UNIFORM_INT);
    } else if (value.Is<int>()) {
        int intValue = value.As<int>();
        ::SetShaderValue(native_->shader, location, &intValue, SHADER_UNIFORM_INT);
    } else if (value.Is<float>()) {
        float floatValue = value.As<float>();
        ::SetShaderValue(native_->shader, location, &floatValue, SHADER_UNIFORM_FLOAT);
    } else if (value.Is<Vector2>()) {
        Vector2 v = value.As<Vector2>();
        float components[2] = {v.x, v.y};
        ::SetShaderValue(native_->shader, location, components, SHADER_UNIFORM_VEC2);
    } else if (value.Is<Vector3>()) {
        Vector3 v = value.As<Vector3>();
        float components[3] = {v.x, v.y, v.z};
        ::SetShaderValue(native_->shader, location, components, SHADER_UNIFORM_VEC3);
    } else if (value.Is<Vector4>()) {
        Vector4 v = value.As<Vector4>();
        float components[4] = {v.x, v.y, v.z, v.w};
        ::SetShaderValue(native_->shader, location, components, SHADER_UNIFORM_VEC4);
    }
}

void Shader::SetFloatArray(const std::string& name, const float* data, int count, int components) {
    if (!native_ || native_->shader.id == 0 || count <= 0 || components < 1 || components > 4) return;
    auto it = native_->locations.find(name);
    if (it == native_->locations.end()) {
        it = native_->locations.emplace(name, ::GetShaderLocation(native_->shader, name.c_str())).first;
    }
    if (it->second == -1) return;
    static constexpr int kTypes[4] = {SHADER_UNIFORM_FLOAT, SHADER_UNIFORM_VEC2, SHADER_UNIFORM_VEC3, SHADER_UNIFORM_VEC4};
    ::SetShaderValueV(native_->shader, it->second, data, kTypes[components - 1], count);
}

void Shader::SetTexture(const std::string& name, unsigned int textureId) {
    if (!native_ || native_->shader.id == 0 || textureId == 0) return;
    auto it = native_->locations.find(name);
    if (it == native_->locations.end()) {
        it = native_->locations.emplace(name, ::GetShaderLocation(native_->shader, name.c_str())).first;
    }
    if (it->second == -1) return;
    ::Texture2D texture{};
    texture.id = textureId;
    ::SetShaderValueTexture(native_->shader, it->second, texture);
}

void Shader::SetTextureUnit(const std::string& name, unsigned int textureId, int unit) {
    if (!native_ || native_->shader.id == 0 || textureId == 0) return;
    auto it = native_->locations.find(name);
    if (it == native_->locations.end()) {
        it = native_->locations.emplace(name, ::GetShaderLocation(native_->shader, name.c_str())).first;
    }
    if (it->second == -1) return;
    ::SetShaderValue(native_->shader, it->second, &unit, SHADER_UNIFORM_INT);
    rlActiveTextureSlot(unit);
    rlEnableTexture(textureId);
    rlActiveTextureSlot(0);
}

}  // namespace Elysium

#pragma once

#include <string>
#include <variant>
#include "Core/Graphics.h"
#include "Core/MathTypes.h"

namespace Elysium {

// A dynamically-typed scalar/vector, used wherever a value needs to cross a
// string-typed boundary (XML attributes, Lua) on its way to a shader uniform —
// see ShaderComponent's overrides map and Shader::SetUniform. Colors are stored
// as a Vector4 in 0..1 float space, matching how a GLSL uniform receives them.
class Value {
   public:
    using Storage = std::variant<bool, int, float, Vector2, Vector3, Vector4>;

    Value() : data_(0.0f) {}
    Value(bool v) : data_(v) {}
    Value(int v) : data_(v) {}
    Value(float v) : data_(v) {}
    Value(Vector2 v) : data_(v) {}
    Value(Vector3 v) : data_(v) {}
    Value(Vector4 v) : data_(v) {}
    Value(Color c)
        : data_(Vector4{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f}) {}

    const Storage& Data() const { return data_; }

    bool operator==(const Value& other) const { return data_ == other.data_; }
    bool SameType(const Value& other) const { return data_.index() == other.data_.index(); }

    template <typename T>
    bool Is() const {
        return std::holds_alternative<T>(data_);
    }

    template <typename T>
    T As() const {
        return std::get<T>(data_);
    }

    // "bool"/"int"/"float"/"vec2"/"vec3"/"vec4" — matches a Uniform's XML type=
    // attribute and ShaderUniform::typeName.
    std::string TypeName() const;

    // Inverse of FromString — space-separated for vector types.
    std::string ToString() const;

    // Parses a Uniform's value="..." XML text using `typeName` to pick the shape;
    // vector components are whitespace/comma separated ("1.0, 0.5 0.2"). Returns a
    // zero-valued Value of that type if `text` doesn't parse.
    static Value FromString(const std::string& typeName, const std::string& text);

   private:
    Storage data_;
};

}  // namespace Elysium

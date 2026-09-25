#include "Core/Value.h"

#include <cstdlib>
#include <vector>

namespace Elysium {

namespace {

std::vector<float> ParseFloats(const std::string& text) {
    std::vector<float> out;
    const char* p = text.c_str();
    char* end = nullptr;
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '\t') p++;
        if (!*p) break;
        float value = std::strtof(p, &end);
        if (end == p) break;  // not a number — stop rather than spin
        out.push_back(value);
        p = end;
    }
    return out;
}

}  // namespace

std::string Value::TypeName() const {
    if (Is<bool>()) return "bool";
    if (Is<int>()) return "int";
    if (Is<float>()) return "float";
    if (Is<Vector2>()) return "vec2";
    if (Is<Vector3>()) return "vec3";
    return "vec4";
}

std::string Value::ToString() const {
    if (Is<bool>()) return As<bool>() ? "true" : "false";
    if (Is<int>()) return std::to_string(As<int>());
    if (Is<float>()) return std::to_string(As<float>());
    // Vector types: x, y, z, w are laid out contiguously.
    const float* f = Is<Vector2>() ? &std::get<Vector2>(data_).x
                   : Is<Vector3>() ? &std::get<Vector3>(data_).x
                                   : &std::get<Vector4>(data_).x;
    const size_t count = Is<Vector2>() ? 2 : Is<Vector3>() ? 3 : 4;
    std::string text;
    for (size_t i = 0; i < count; ++i) text += (i ? " " : "") + std::to_string(f[i]);
    return text;
}

Value Value::FromString(const std::string& typeName, const std::string& text) {
    if (typeName == "bool") {
        return Value(text == "true" || text == "1");
    }
    if (typeName == "int") {
        return Value(std::atoi(text.c_str()));
    }
    if (typeName == "vec2" || typeName == "vec3" || typeName == "vec4") {
        std::vector<float> c = ParseFloats(text);
        c.resize(4, 0.0f);  // missing components are zero
        if (typeName == "vec2") return Value(Vector2{c[0], c[1]});
        if (typeName == "vec3") return Value(Vector3{c[0], c[1], c[2]});
        return Value(Vector4{c[0], c[1], c[2], c[3]});
    }
    // "float" and anything unrecognized.
    return Value(std::strtof(text.c_str(), nullptr));
}

}  // namespace Elysium

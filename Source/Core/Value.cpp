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
    if (Is<Vector2>()) {
        Vector2 v = As<Vector2>();
        return std::to_string(v.x) + " " + std::to_string(v.y);
    }
    if (Is<Vector3>()) {
        Vector3 v = As<Vector3>();
        return std::to_string(v.x) + " " + std::to_string(v.y) + " " + std::to_string(v.z);
    }
    Vector4 v = As<Vector4>();
    return std::to_string(v.x) + " " + std::to_string(v.y) + " " + std::to_string(v.z) + " " +
           std::to_string(v.w);
}

Value Value::FromString(const std::string& typeName, const std::string& text) {
    if (typeName == "bool") {
        return Value(text == "true" || text == "1");
    }
    if (typeName == "int") {
        return Value(std::atoi(text.c_str()));
    }
    if (typeName == "vec2") {
        std::vector<float> c = ParseFloats(text);
        return Value(Vector2{c.size() > 0 ? c[0] : 0.0f, c.size() > 1 ? c[1] : 0.0f});
    }
    if (typeName == "vec3") {
        std::vector<float> c = ParseFloats(text);
        return Value(Vector3{c.size() > 0 ? c[0] : 0.0f, c.size() > 1 ? c[1] : 0.0f,
                              c.size() > 2 ? c[2] : 0.0f});
    }
    if (typeName == "vec4") {
        std::vector<float> c = ParseFloats(text);
        return Value(Vector4{c.size() > 0 ? c[0] : 0.0f, c.size() > 1 ? c[1] : 0.0f,
                              c.size() > 2 ? c[2] : 0.0f, c.size() > 3 ? c[3] : 0.0f});
    }
    // "float" and anything unrecognized.
    return Value(std::strtof(text.c_str(), nullptr));
}

}  // namespace Elysium

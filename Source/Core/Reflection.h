#pragma once

#include <concepts>
#include <initializer_list>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "Core/AssetKind.h"
#include "Core/Entity.h"
#include "Core/Graphics.h"
#include "Core/Math/MathTypes.h"

namespace Elysium {

// Typed component fields. A component lists its fields once:
//
//```
// static FieldList Fields() {
//     return {
//         Field("Width", &RectangleComponent::width, "width").Speed(1.0f).Range(1.0f, 1000.0f),
//         Field("Sprite", &SpriteComponent::spriteName, "spriteName").Asset(AssetKind::Sprite),
//     };
// }
//```
//
// and gets from it typed prefab parameters, and in the editor a generic Inspector section
// (Editor/Inspectors/FieldInspector.h). A field's `key` is its XML attribute,
// the name prefab parameters and overrides address it by; a field without one is runtime
// state, shown read-only and never exposed.
// Choice is an int member restricted to a named set, drawn as a dropdown rather than a drag.
enum class FieldType { Bool, Int, Float, Vector2, Color, String, Text, Asset, Choice };

struct FieldInfo {
    std::string label;
    std::string key;  // XML attribute; empty = runtime state
    FieldType type = FieldType::Float;
    AssetKind asset = AssetKind::Folder;  // for FieldType::Asset
    float speed = 1.0f, min = 0.0f, max = 0.0f;  // drag widgets; min == max means unbounded
    bool readOnly = false;
    std::vector<std::string> choices;  // for FieldType::Choice; the value indexes this
    std::string section;  // a heading drawn before the first field of a new section
    std::function<void*(void*)> address;  // component -> field

    FieldInfo Speed(float s) && { speed = s; return std::move(*this); }
    FieldInfo Range(float lo, float hi) && { min = lo; max = hi; return std::move(*this); }
    FieldInfo ReadOnly() && { readOnly = true; return std::move(*this); }
    FieldInfo Section(const char* s) && { section = s; return std::move(*this); }
    FieldInfo Multiline() && { type = FieldType::Text; return std::move(*this); }
    FieldInfo Asset(AssetKind kind) && { type = FieldType::Asset; asset = kind; return std::move(*this); }
    // Restricts an int field to a named set, drawn as a dropdown. The stored value is the index,
    // so the labels can be reworded without touching saved data.
    FieldInfo Choices(std::initializer_list<const char*> labels) && {
        type = FieldType::Choice;
        for (const char* label : labels) choices.emplace_back(label);
        return std::move(*this);
    }

    bool Serialized() const { return !key.empty(); }
};

using FieldList = std::vector<FieldInfo>;

// A field of component C, typed from the member. Without a key it's runtime state and
// read-only.
template <typename C, typename M>
FieldInfo Field(const char* label, M C::*member, const char* key = "") {
    FieldInfo info;
    info.label = label;
    info.key = key;
    info.readOnly = !*key;
    if constexpr (std::same_as<M, bool>) info.type = FieldType::Bool;
    else if constexpr (std::same_as<M, int>) info.type = FieldType::Int;
    else if constexpr (std::same_as<M, float>) info.type = FieldType::Float;
    else if constexpr (std::same_as<M, Vector2>) info.type = FieldType::Vector2;
    else if constexpr (std::same_as<M, Color>) info.type = FieldType::Color;
    else if constexpr (std::same_as<M, std::string>) info.type = FieldType::String;
    else static_assert(sizeof(M) == 0, "Unsupported field type");
    info.address = [member](void* component) -> void* { return &(static_cast<C*>(component)->*member); };
    return info;
}

template <typename T>
concept Reflected = requires {
    { T::Fields() } -> std::convertible_to<std::vector<FieldInfo>>;
};

}  // namespace Elysium

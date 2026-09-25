#pragma once

#include <string>
#include <vector>

#include "imgui.h"

#include "Core/Entity.h"
#include "Core/ServiceLocator.h"
#include "Core/Value.h"

namespace Elysium {

struct ApplicationConfig;

template<typename T>
concept Inspectable = requires(T& c, Entity e, ServiceLocator& services) {
    { T::Inspect(c, e, services) } -> std::same_as<void>;
};

// One widget for a Value, picked by its type; vec3/vec4 use a color picker when asColor.
// Returns true when edited.
inline bool InspectValue(const char* id, Value& value, bool asColor = false) {
    if (value.Is<bool>()) {
        bool v = value.As<bool>();
        if (!ImGui::Checkbox(id, &v)) return false;
        value = Value(v);
    } else if (value.Is<int>()) {
        int v = value.As<int>();
        if (!ImGui::DragInt(id, &v)) return false;
        value = Value(v);
    } else if (value.Is<float>()) {
        float v = value.As<float>();
        if (!ImGui::DragFloat(id, &v, 0.05f)) return false;
        value = Value(v);
    } else if (value.Is<Vector2>()) {
        Vector2 v = value.As<Vector2>();
        if (!ImGui::DragFloat2(id, &v.x, 0.05f)) return false;
        value = Value(v);
    } else if (value.Is<Vector3>()) {
        Vector3 v = value.As<Vector3>();
        if (!(asColor ? ImGui::ColorEdit3(id, &v.x) : ImGui::DragFloat3(id, &v.x, 0.05f))) return false;
        value = Value(v);
    } else {
        Vector4 v = value.As<Vector4>();
        if (!(asColor ? ImGui::ColorEdit4(id, &v.x) : ImGui::DragFloat4(id, &v.x, 0.05f))) return false;
        value = Value(v);
    }
    return true;
}

// A Value row: name (grey while at `fallback`), widget, and a Reset button enabled off the
// default. Returns true when edited or reset; `value` holds the result.
inline bool InspectValueRow(const std::string& name, Value& value, const Value& fallback, bool isDefault,
                            bool asColor = false) {
    ImGui::PushID(name.c_str());
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(isDefault ? ImVec4(0.6f, 0.6f, 0.6f, 1) : ImVec4(1, 1, 1, 1), "%s", name.c_str());
    ImGui::SameLine(160.0f);
    ImGui::SetNextItemWidth(-56);
    bool changed = InspectValue("##value", value, asColor);
    ImGui::SameLine();
    ImGui::BeginDisabled(isDefault);
    if (ImGui::SmallButton("Reset")) { value = fallback; changed = true; }
    ImGui::EndDisabled();
    ImGui::PopID();
    return changed;
}

// Combo over `options` plus a leading "<None>" (the empty path). Returns true when changed.
inline bool InspectPathCombo(const char* id, std::string& path, const std::vector<std::string>& options) {
    bool changed = false;
    if (ImGui::BeginCombo(id, path.empty() ? "<None>" : path.c_str())) {
        for (size_t i = 0; i <= options.size(); ++i) {
            const std::string option = i == 0 ? "" : options[i - 1];
            const std::string label = (i == 0 ? std::string("<None>") : option) + "##" + std::to_string(i);
            const bool isSelected = option == path;
            if (ImGui::Selectable(label.c_str(), isSelected) && !isSelected) { path = option; changed = true; }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

class Editor {
   public:
    Editor(ServiceLocator& services, const std::string& name) : services_(services), name_(name) {}
    virtual ~Editor() = default;

    virtual void Initialize(const ApplicationConfig& config) {}
    virtual void Draw() = 0;

    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
    void ToggleVisibility() { isVisible_ = !isVisible_; }
    const std::string& GetName() const { return name_; }

   protected:
    ServiceLocator& services_;
    std::string name_;
    bool isVisible_ = false;
};

}  // namespace Elysium

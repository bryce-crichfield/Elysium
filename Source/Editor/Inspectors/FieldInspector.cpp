#include "Editor/Inspectors/FieldInspector.h"

#include <cstdio>
#include <cstring>

#include "Core/Reflection.h"
#include "Core/Xml.h"
#include "Editor/Widgets/AssetField.h"
#include "Editor/Style/AssetStyle.h"
#include "Editor/Widgets/Widgets.h"

namespace Elysium {

namespace {

bool EditString(const char* id, std::string& value, bool multiline) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    const bool changed = multiline ? ImGui::InputTextMultiline(id, buffer, sizeof(buffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4))
                                   : ImGui::InputText(id, buffer, sizeof(buffer));
    if (changed) value = buffer;
    return changed;
}

// A dropdown over the field's named set. The value is the index, clamped so a stale one out of
// range shows the first entry rather than reading past the list.
bool EditChoice(const char* id, const FieldInfo& field, int& value) {
    if (field.choices.empty()) return false;
    if (value < 0 || value >= (int)field.choices.size()) value = 0;
    if (!ImGui::BeginCombo(id, field.choices[value].c_str())) return false;
    bool changed = false;
    for (int i = 0; i < (int)field.choices.size(); i++) {
        if (!ImGui::Selectable(field.choices[i].c_str(), i == value) || i == value) continue;
        value = i;
        changed = true;
    }
    ImGui::EndCombo();
    return changed;
}

bool EditColor(const char* id, Color& color) {
    float rgba[4] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
    if (!ImGui::ColorEdit4(id, rgba)) return false;
    auto byte = [](float v) { return (unsigned char)(v * 255.0f + 0.5f); };
    color = {byte(rgba[0]), byte(rgba[1]), byte(rgba[2]), byte(rgba[3])};
    return true;
}

// One field's widget on its live value.
bool EditField(const char* id, const FieldInfo& field, void* value) {
    const float lo = field.min, hi = field.max;
    switch (field.type) {
        case FieldType::Bool: return ImGui::Checkbox(id, static_cast<bool*>(value));
        case FieldType::Int: return ImGui::DragInt(id, static_cast<int*>(value), field.speed, (int)lo, (int)hi);
        case FieldType::Float: return ImGui::DragFloat(id, static_cast<float*>(value), field.speed, lo, hi);
        case FieldType::Vector2: return ImGui::DragFloat2(id, &static_cast<Vector2*>(value)->x, field.speed, lo, hi);
        case FieldType::Color: return EditColor(id, *static_cast<Color*>(value));
        case FieldType::String: return EditString(id, *static_cast<std::string*>(value), false);
        case FieldType::Text: return EditString(id, *static_cast<std::string*>(value), true);
        case FieldType::Asset: return AssetField(id, field.asset, *static_cast<std::string*>(value));
        case FieldType::Choice: return EditChoice(id, field, *static_cast<int*>(value));
    }
    return false;
}

std::string FormatColor(Color c) {
    char text[16];
    snprintf(text, sizeof(text), "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    return text;
}

}  // namespace

void FieldTypeBadge(const FieldInfo* field) {
    const auto& palette = Editor::Palette();
    const char* icon = ICON_FA_QUESTION;
    const char* name = "Untyped (edited as text)";
    ImVec4 color = palette.TextMuted;
    if (field) {
        switch (field->type) {
            case FieldType::Bool: icon = ICON_FA_TOGGLE_ON; name = "Bool"; break;
            case FieldType::Int: icon = ICON_FA_HASHTAG; name = "Int"; break;
            case FieldType::Float: icon = ICON_FA_SLIDERS; name = "Float"; break;
            case FieldType::Vector2: icon = ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT; name = "Vector2"; break;
            case FieldType::Color: icon = ICON_FA_PALETTE; name = "Color"; break;
            case FieldType::String: icon = ICON_FA_FONT; name = "String"; break;
            case FieldType::Text: icon = ICON_FA_ALIGN_LEFT; name = "Text"; break;
            case FieldType::Choice: icon = ICON_FA_LIST; name = "Choice"; break;
            case FieldType::Asset: {
                const AssetStyle style = StyleOf(field->asset);
                icon = style.icon;
                name = style.label;
                color = style.color;
                break;
            }
        }
    }
    ImGui::AlignTextToFramePadding();
    ColoredText(color, icon);
    ItemTooltip(name);
}

bool InspectFields(void* component, const std::vector<FieldInfo>& fields) {
    bool changed = false;
    std::string section;
    for (size_t i = 0; i < fields.size(); ++i) {
        const FieldInfo& field = fields[i];
        if (field.section != section) {
            section = field.section;
            if (!section.empty()) SectionHeader(section.c_str());
        }
        ImGui::PushID((int)i);
        PropertyLabel(field.label.c_str(), field.readOnly);
        ImGui::BeginDisabled(field.readOnly);
        changed |= EditField("##value", field, field.address(component));
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    return changed;
}

bool InspectFieldText(const char* id, const FieldInfo& field, std::string& text) {
    // Parse the text into the field's type, edit that, and write it back the way the
    // component's LoadXml reads it.
    switch (field.type) {
        case FieldType::Bool: {
            bool v = text == "true" || text == "1";
            if (!ImGui::Checkbox(id, &v)) return false;
            text = v ? "true" : "false";
            return true;
        }
        case FieldType::Choice:
        case FieldType::Int: {
            int v = std::atoi(text.c_str());
            if (!EditField(id, field, &v)) return false;
            text = std::to_string(v);
            return true;
        }
        case FieldType::Float: {
            float v = (float)std::atof(text.c_str());
            if (!EditField(id, field, &v)) return false;
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%g", v);
            text = buffer;
            return true;
        }
        case FieldType::Color: {
            Color c = ParseHexColor(text, Colors::White);
            if (!EditColor(id, c)) return false;
            text = FormatColor(c);
            return true;
        }
        case FieldType::Vector2: {
            // Not a single attribute; edited as text.
            return EditString(id, text, false);
        }
        case FieldType::String:
        case FieldType::Text:
        case FieldType::Asset:
            return EditField(id, field, &text);
    }
    return false;
}

}  // namespace Elysium

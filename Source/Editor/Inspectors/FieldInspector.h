#pragma once

#include <string>
#include <vector>

#include "Core/Reflection.h"

namespace Elysium {

// The Inspector rows for `fields` of `component`. Returns true when any was edited.
bool InspectFields(void* component, const std::vector<FieldInfo>& fields);

// One field's widget over its serialized (XML attribute) text, for values that live as text,
// like prefab parameters. Returns true when edited; `text` holds the result.
bool InspectFieldText(const char* id, const FieldInfo& field, std::string& text);

// A field's type as a small icon in its color (an asset's kind color), with the type as a
// tooltip; `field` null means untyped.
void FieldTypeBadge(const FieldInfo* field);

}  // namespace Elysium

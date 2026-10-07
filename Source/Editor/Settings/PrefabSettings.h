#pragma once

#include "Core/ServiceLocator.h"

namespace Elysium {

class EditorApplication;
struct EditorDocument;

// A prefab tab's settings, shown in the Viewport in place of the rendered prefab: its
// exposed parameters. Adding one takes a field of the entity selected in the Hierarchy.
class PrefabSettings {
public:
    explicit PrefabSettings(EditorApplication& editor);

    void Draw(EditorDocument& document);

private:
    EditorApplication& editor_;
    ServiceLocator& services_;
};

}  // namespace Elysium

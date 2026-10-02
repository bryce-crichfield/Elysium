#pragma once

#include "Core/ServiceLocator.h"

namespace Elysium {

namespace Services {
struct EditorDocument;
}

// A prefab tab's settings, shown in the Viewport in place of the rendered prefab: its
// exposed parameters. Adding one takes a field of the entity selected in the Hierarchy.
class PrefabSettings {
public:
    explicit PrefabSettings(ServiceLocator& services) : services_(services) {}

    void Draw(Services::EditorDocument& document);

private:
    ServiceLocator& services_;
};

}  // namespace Elysium

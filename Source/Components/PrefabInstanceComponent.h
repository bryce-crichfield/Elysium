#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    // Tags an entity as having been spawned from a prefab file rather than authored
    // directly in scene XML. SceneSaver groups entities by (src, instanceId) and diffs
    // each one's current component values against the prefab's current defaults to
    // write out only what changed, instead of flattening the entity to a plain <Entity>.
    //
    // Deliberately not XmlLoadable/XmlSavable: it never appears as a tag inside an
    // <Entity> block. It's stamped on entities by the prefab-instance loader and
    // reconstructed at save time from the <PrefabInstance>/<Override> grouping, not
    // round-tripped through XML itself.
    struct PrefabInstanceComponent {
        std::string src;          // Prefab file path, relative to the scene file's directory.
        std::string instanceId;   // The placement's unique id within the scene (the <PrefabInstance id="..."> value).
        int localEntityId = -1;   // This entity's `id` within the prefab file.

        static constexpr const char* Name() { return "PrefabInstance"; }

        static void Inspect(PrefabInstanceComponent& c, Entity e);
    };
}

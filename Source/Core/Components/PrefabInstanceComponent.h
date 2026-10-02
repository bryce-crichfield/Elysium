#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    // Tags an entity as spawned from a prefab placement (<PrefabInstance>) instead of
    // authored directly. Savers skip tagged entities and instead write one <PrefabInstance>
    // per instanceId with only the fields that differ from the prefab's defaults.
    //
    // Not XmlLoadable/XmlSavable: it's stamped by the prefab loader, never written as a tag.
    struct PrefabInstanceComponent {
        std::string src;          // Prefab path as written, relative to ownerDir.
        std::string ownerDir;     // Directory of the scene/prefab file that places this instance.
        std::string instanceId;   // The placement's unique id (<PrefabInstance id="...">).
        int localEntityId = -1;   // This entity's id within the prefab; -1 for entities of nested instances.

        std::string FullPath() const { return ownerDir + src; }

        static constexpr const char* Name() { return "Prefab Instance"; }

    };
}

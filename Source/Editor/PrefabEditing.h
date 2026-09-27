#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <tinyxml2.h>
#include "Core/Entity.h"
#include "Core/Prefab.h"
#include "Core/PrefabInstance.h"

namespace Elysium {

class World;
class Scene;
class ServiceLocator;

// Editor-only prefab operations: placing prefabs, saving prefab documents, and refreshing
// placements after a prefab changes. The game only ever loads prefabs (Core/Prefab.h).
namespace PrefabEditing {

// Live refresh of placements after their prefab file changes: snapshot the matching
// placements (overrides diffed against the *current* prefab) before rewriting the file,
// then respawn them from the snapshot afterward. Entities parented to a placement from
// outside are re-attached to the new root.
std::unique_ptr<tinyxml2::XMLDocument> Snapshot(World* world, ServiceLocator& services, const Scene* host,
                                                const PrefabInstances::Filter& filter);
void Respawn(World* world, tinyxml2::XMLDocument& snapshot, ServiceLocator& services,
             const PrefabInstances::Filter& filter);

// An instance id not used by any placement in `world`: base, base2, base3...
std::string UniqueInstanceId(World* world, const std::string& base);

// Spawns a new placement of the prefab at `fullPath` into `world`, with src written relative
// to `ownerDir` (the directory of the scene/prefab file that will save it). Returns the root.
Entity Instantiate(World* world, const std::string& fullPath, const std::string& ownerDir, ServiceLocator& services);

// Writes a prefab edited directly (spawned raw) back to disk and reloads its asset.
// `localIds` preserves each entity's original id so overrides in scenes keep pointing at the
// right entity; entities without one get fresh ids. `host` settles placement defaults (see
// PrefabInstances::Save).
bool SaveFile(World* world, const std::string& fullPath, std::unordered_map<Entity, int>& localIds,
              const std::vector<PrefabParameter>& parameters, ServiceLocator& services, const Scene* host = nullptr);

// Writes `root` and its subtree from `world` as a new prefab file, `root` becoming the
// prefab's root (id 0). Placements inside the subtree become nested placements.
bool SaveSubtree(World* world, Entity root, const std::string& fullPath, ServiceLocator& services,
                 const Scene* host = nullptr);

}  // namespace PrefabEditing

}  // namespace Elysium

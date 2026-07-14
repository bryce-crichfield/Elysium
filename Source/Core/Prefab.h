#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <tinyxml2.h>
#include "Core/Entity.h"

namespace Elysium {

class World;

// One <Override entity="..." component="..." field="..." value="..."/> as written to
// (or read from) scene XML.
struct PrefabOverride {
    int entity = -1;
    std::string component;
    std::string field;
    std::string value;
};

// Local `id` (as authored on <Entity id="N"> in a prefab file) -> spawned Entity.
using PrefabIdMap = std::unordered_map<int, Entity>;

// Loads every child component of a single <Entity> block onto `entity`, dispatching
// through the ComponentRegistry the same way for hand-authored scene entities and
// prefab entities. Shared so both loaders apply identical component semantics
// (including the CameraComponent -> FollowComponent/ParentComponent special case).
void LoadEntityComponents(tinyxml2::XMLElement* xmlEntity, World* world, Entity entity);

// Loads a prefab file's <Entities> root from disk. `outDoc` must outlive
// `outEntitiesRoot`. Returns false (and logs) if the file is missing or unparsable.
bool LoadPrefabFile(const std::string& fullPath, tinyxml2::XMLDocument& outDoc, tinyxml2::XMLElement*& outEntitiesRoot);

// Spawns every <Entity id="N"> under `entitiesRoot` into `world` via LoadEntityComponents.
//
// If `instanceId` is non-empty, every spawned entity's NameComponent (if present) is
// namespaced to "<instanceId>::<name>" so multiple placements of the same prefab don't
// collide. Pass "" to load a prefab's raw, unnamespaced entities (used when editing a
// prefab file directly, and internally when reconstructing prefab defaults for diffing).
//
// ParentComponent targets that parse as an integer matching another entity's local `id`
// within this same call are resolved immediately via direct parent/child linkage —
// intra-prefab hierarchy never depends on name-based lookup, so it can't be broken by
// the namespacing above, and targetName is left untouched so re-saving a raw prefab
// reproduces it exactly. Any other target is left for the caller's normal by-name
// hierarchy resolution pass (cross-prefab / hand-authored references).
PrefabIdMap LoadPrefabEntities(tinyxml2::XMLElement* entitiesRoot, World* world, const std::string& instanceId);

// Applies one override from a <PrefabInstance>'s <Override> list. Logs and drops it if
// `localId` isn't in `idToEntity` (prefab entity removed from source since the override
// was written) or if `component` isn't a known overridable component.
void ApplyPrefabOverride(World* world, const PrefabIdMap& idToEntity, int localId,
                          const std::string& component, const std::string& field, const std::string& value);

// Writes `world`'s living entities back out as a prefab file: a plain
// <Entities><Entity id="N">...</Entity></Entities> document, id assigned by
// living-entity order. Used when saving a prefab edited directly in the editor.
bool SavePrefabFile(World* world, const std::string& fullPath);

// Diffs one live (instantiated) entity against its freshly-loaded prefab-default
// counterpart, component by component, reusing each component's own XmlSaver as the
// diffing substrate. Produces one PrefabOverride per attribute whose live value differs
// from the default value. Structural differences (a component present on only one side)
// aren't representable as field-level overrides and are skipped — see the design doc's
// "detach from prefab" open question for lifting this limitation.
std::vector<PrefabOverride> DiffPrefabEntity(int localId, World* liveWorld, Entity liveEntity,
                                              World* defaultWorld, Entity defaultEntity);

}  // namespace Elysium

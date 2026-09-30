#pragma once

#include <functional>
#include <string>
#include <vector>
#include <tinyxml2.h>
#include "Core/Entity.h"
#include "Core/Prefab.h"

namespace Elysium {

class World;
class Scene;
class ServiceLocator;
class XMLBuilder;
struct PrefabInstanceComponent;

// Placements of prefabs in a world: <PrefabInstance src id> blocks in scene/prefab XML on
// one side, entities tagged with PrefabInstanceComponent on the other. These act on a
// world's tagged entities rather than on a Prefab, so they're plain functions.
namespace PrefabInstances {

// Selects placements by their tag (e.g. "instances of Unit.xml"). Empty = all.
using Filter = std::function<bool(const PrefabInstanceComponent&)>;

// Loads every <PrefabInstance> directly under `parent`: spawns it, tags its entities with
// PrefabInstanceComponent and applies its <Param>s/<Override>s. `ownerDir` is the directory
// `src` is relative to. `namePrefix` namespaces instance ids (for nested instances).
std::vector<Entity> Load(tinyxml2::XMLElement* parent, World* world, const std::string& ownerDir,
                         ServiceLocator& services, const std::string& namePrefix = "");

// Writes the placements in `world` as <PrefabInstance> blocks under `builder`. A placement
// is a black box, so only the instance root's fields and exposed parameters are written,
// diffed against the prefab's defaults. With `host`, defaults are first settled through its
// paused systems so values those systems derive (sprite rect size, world transforms) don't
// read as overrides.
void Save(XMLBuilder& builder, World* world, ServiceLocator& services, const Scene* host = nullptr,
          const Filter& filter = {});

// Applies one field override. Logs and drops it if the entity or component is unknown.
void ApplyOverride(World* world, const PrefabIdMap& idToEntity, const PrefabOverride& override, ServiceLocator& services);

// Reads a single field's current serialized value ("" if absent).
std::string ReadField(World* world, Entity entity, const std::string& component, const std::string& field);

// A placed prefab is a black box in the editor: only its root (the instance entity whose
// parent isn't part of the same instance) is shown and selectable; the rest is internal.
bool IsRoot(const World& world, Entity entity);
bool IsInternal(const World& world, Entity entity);
// The instance root `entity` belongs to, or `entity` itself if it isn't inside an instance.
Entity RootOf(const World& world, Entity entity);

// Gives the placements among `entities` fresh instance ids, renaming their entities to match.
// A copy of a placement has to become a placement of its own: two placements sharing an id are
// grouped into a single <PrefabInstance> on save, so one of them would simply disappear.
void Reinstance(World* world, const std::vector<Entity>& entities);

}  // namespace PrefabInstances

}  // namespace Elysium

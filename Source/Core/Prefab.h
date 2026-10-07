#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <tinyxml2.h>
#include "Core/Entity.h"

namespace Elysium {

class World;
class ServiceLocator;
namespace Services { class IAssetService; }

// A prefab is a reusable subgraph of entities stored in its own XML file:
//
//```
//<Prefab>
//    <Parameters>
//        <Parameter name="Name" entity="0" component="NameComponent" field="name"/>
//    </Parameters>
//    <Entities>
//        <Entity id="0"> ...components... </Entity>
//        <Entity id="1"> <ParentComponent target="0"/> ... </Entity>
//    </Entities>
//    <!-- Composition -->
//    <PrefabInstance src="Other.xml" id="Nested"/>
//</Prefab>
//```
//
// Scenes and other prefabs place a prefab with
//
//```
//<PrefabInstance src="Prefabs/Unit.xml" id="Knight1">
//    <Param name="Name" value="Knight1"/>
//    <Override entity="0" component="TransformComponent" field="x" value="320"/>
//</PrefabInstance>
//```
//
// Prefabs are assets (payload `Prefab`, loaded by PrefabAsset), fetched synchronously with
// IAssetService::LoadAssetNow since scene loading needs them immediately. Everything not
// overridden comes from the prefab, so editing one propagates to its placements.
// Placements themselves are handled in Core/PrefabInstance.h.

// One exposed field: a friendly name for (entity, component, field) inside the prefab.
struct PrefabParameter {
    std::string name;
    int entity = 0;
    std::string component;
    std::string field;
};

// One <Override>/<Param> as written to (or read from) a PrefabInstance.
struct PrefabOverride {
    int entity = -1;
    std::string component;
    std::string field;
    std::string value;
};

// Local `id` (as authored on <Entity id="N">) -> spawned Entity.
using PrefabIdMap = std::unordered_map<int, Entity>;

struct PrefabSpawnResult {
    PrefabIdMap ids;              // direct entities by local id
    std::vector<Entity> spawned;  // everything spawned, including nested instances' entities
};

// A parsed prefab file. Owns its XML; spawning reads entities straight from it through
// the same component loaders scenes use.
class Prefab {
   public:
    Prefab() = default;
    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;

    // The prefab at `fullPath`, loaded (or reused) through the asset service. Null (and
    // logged) if it's missing or malformed.
    static const Prefab* Get(Services::IAssetService& assets, const std::string& fullPath);
    // The same, for the game: playing, a prefab not already loaded is read from disk on the
    // spot, stalling the frame, so it warns that it belongs in the scene's <Preload>.
    static const Prefab* Get(ServiceLocator& services, const std::string& fullPath);
    // Rereads it from disk, e.g. after the editor saved it. Earlier pointers dangle.
    static const Prefab* Reload(Services::IAssetService& assets, const std::string& fullPath);

    // Parses the file. Logs and returns false if missing or malformed. Thread-safe (runs
    // on an asset worker thread for async loads).
    bool Load(const std::string& fullPath);

    const std::string& GetFullPath() const { return fullPath_; }
    const std::vector<PrefabParameter>& GetParameters() const { return parameters_; }
    const PrefabParameter* FindParameter(const std::string& name) const;
    const PrefabParameter* FindParameter(int entity, const std::string& component, const std::string& field) const;

    // Spawns the prefab's entities (and its nested <PrefabInstance>s) into `world`.
    //
    // If `instanceId` is non-empty, NameComponents are namespaced "<instanceId>::<name>" so
    // multiple placements don't collide. Pass "" to load a prefab raw (for editing it directly).
    // Numeric ParentComponent targets are resolved against local ids immediately; anything else
    // is left for the caller's by-name hierarchy pass. A prefab has exactly one root
    // (<Entity id="0">, else the first); other parentless entities are rejected.
    PrefabSpawnResult Spawn(World* world, const std::string& instanceId, ServiceLocator& services) const;

    // True if this prefab is the file at `fullPath` or places it (directly or nested).
    bool DependsOn(const std::string& fullPath, Services::IAssetService& assets) const;

   private:
    std::string fullPath_;
    tinyxml2::XMLDocument doc_;
    tinyxml2::XMLElement* root_ = nullptr;      // <Prefab>
    tinyxml2::XMLElement* entities_ = nullptr;  // <Entities>
    std::vector<PrefabParameter> parameters_;
};

}  // namespace Elysium

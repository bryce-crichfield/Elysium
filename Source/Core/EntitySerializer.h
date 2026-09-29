#pragma once

#include <string>
#include <vector>
#include "Core/Entity.h"

namespace Elysium {

class ServiceLocator;
class World;

// XML round-tripping of live entities and components, for the editor's undo history and its
// clipboard. Everything here dispatches through ComponentRegistry's registered savers and
// loaders, so it covers every component that round-trips through scene XML and needs no
// per-component code of its own — including the ones with hand-written inspectors, which a
// per-field approach would miss.
namespace EntityXml {

// One component of one entity as a standalone XML element ("<TransformComponent x=... />").
// Empty when the entity doesn't have it, when its saver wrote nothing, or when the component
// doesn't round-trip through XML. `component` is the component's XML tag.
std::string SaveComponent(World& world, Entity entity, const std::string& component);

// Puts `xml` back onto `entity`, adding the component if it is missing; an empty `xml` removes
// it. Together with SaveComponent this is the before/after pair a ComponentEditCommand holds,
// and the empty case is how it expresses add-component and remove-component.
void LoadComponent(World& world, Entity entity, const std::string& component,
                   const std::string& xml, ServiceLocator& services);

// `root` and all its descendants as a self-contained document. Parents are recorded as indices
// within the subtree rather than by name, so restoring never depends on entity names being
// unique — which they are not, since prefab placements namespace theirs.
std::string SaveSubtree(World& world, Entity root);

// Recreates what SaveSubtree wrote, parented under `parent` (INVALID_ENTITY for none), and
// returns the new root. INVALID_ENTITY if `xml` can't be parsed. Entity ids are freshly
// allocated and bear no relation to the ones that were saved.
//
// `outCreated` receives every entity made, in the same order SaveSubtree walked them (root
// first, depth-first). Undo uses it to rebind the stable id of each entity in the subtree, not
// just the root, so a command referring to a descendant survives a delete/undo cycle too.
Entity LoadSubtree(World& world, const std::string& xml, Entity parent, ServiceLocator& services,
                   std::vector<Entity>* outCreated = nullptr);

}  // namespace EntityXml
}  // namespace Elysium

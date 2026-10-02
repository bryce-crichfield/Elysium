#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Editor/Commands/EditorCommand.h"
#include "Core/Entity.h"

namespace Elysium {

class World;

// The raw hierarchy edits, recording nothing. Commands call these directly; the service's
// Reparent/ReorderBefore/ReorderAfter record a command which then lands back here. Keeping the
// two apart is what stops a command's Do() from recording another copy of itself.
namespace EditorOps {

// Re-homes `entity` under `parent`, or makes it a root (INVALID_ENTITY), whatever its
// ParentComponent said before.
void Reparent(World& world, Entity entity, Entity parent);

// Moves `entity` to sit immediately before or after `anchor`, updating both orderings the
// Hierarchy reads — the parent's child list and the world's living order — so a reorder looks
// the same whether the entity is nested or at root level.
void PlaceBeside(World& world, Entity entity, Entity anchor, bool before);

}  // namespace EditorOps

// The editor's undoable operations. Deliberately few: because a component round-trips through
// XML, "before and after, as XML" covers every field edit in the editor with one command —
// including the eleven components with hand-written inspectors, which a per-field command
// would have silently missed. Anything more complex is a transaction of these.

// One component of one entity, changed. An empty `before` means the component did not exist
// (so this is an add), and an empty `after` means it no longer does (a remove).
//
// Most callers construct this *after* the change has already been written to the live
// component — the Inspector's widgets and the gizmo both mutate directly and are diffed
// afterwards — so the first Do() is usually re-applying a value that is already in place.
class ComponentEditCommand : public EditorCommand {
   public:
    ComponentEditCommand(EntityRef entity, std::string componentTag, std::string before, std::string after,
                         std::string label);

    void Do(CommandContext& context) override;
    void Undo(CommandContext& context) override;
    const char* Label() const override { return label_.c_str(); }
    // Absorbs a later edit of the same component on the same entity, keeping this command's
    // starting value. This is what makes a gizmo drag or a held DragFloat one undo step.
    bool MergeWith(const EditorCommand& newer) override;

   private:
    EntityRef entity_;
    std::string component_;
    std::string before_;
    std::string after_;
    std::string label_;
};

// Shared machinery for the two commands that differ only in which direction they run: an
// entity subtree exists, or it doesn't.
//
// Recreating a subtree mints new entity ids, so every stable id in it is rebound — not just
// the root's. Without that, undoing a delete would give back the entities but leave any
// earlier command that referenced a *descendant* pointing at nothing.
class SubtreeCommand : public EditorCommand {
   protected:
    // Notes which entity this command is about, without serializing it yet.
    void Bind(CommandContext& context, Entity root);
    // Captures `root` and its descendants as they are now. Must be called while they are
    // still alive.
    void Capture(CommandContext& context, Entity root);
    // Recreates the captured subtree if it isn't already there, and rebinds its stable ids.
    void Restore(CommandContext& context);
    // Destroys it and unbinds those ids.
    void Remove(CommandContext& context);
    // Whether the captured root currently resolves to a live entity.
    bool Exists(CommandContext& context) const;

    EntityRef root_;
    EntityRef parent_;
    std::string xml_;
    // Stable ids of the captured subtree, in the order SaveSubtree walked it.
    std::vector<uint64_t> stableIds_;
};

// An entity that has just been created — by Create Entity, Duplicate, a dropped prefab, a
// paint stroke — recorded so it can be taken away again.
//
// Do() is a no-op the first time, because the caller already did the work and needs the new
// Entity back to keep configuring it. It only recreates on a redo, after an undo removed it.
// That "record what already happened" shape is what lets prefab instantiation and painting
// become undoable without restructuring either of them.
class SpawnCommand : public SubtreeCommand {
   public:
    SpawnCommand(CommandContext& context, Entity created, std::string label);

    void Do(CommandContext& context) override { Restore(context); }
    // Serializes the entity here rather than in the constructor. Callers keep configuring what
    // they have just created: the painter positions it, the Hierarchy reparents it, the
    // Inspector edits it. The state worth bringing back on a redo is the one it had when it
    // was undone, not the bare thing it was a moment after spawning.
    void Undo(CommandContext& context) override;
    const char* Label() const override { return label_.c_str(); }

   private:
    std::string label_;
};

// An entity and its whole subtree, removed. Captures on construction, so build it before the
// entity goes away.
class DeleteEntityCommand : public SubtreeCommand {
   public:
    DeleteEntityCommand(CommandContext& context, Entity entity);

    void Do(CommandContext& context) override { Remove(context); }
    void Undo(CommandContext& context) override { Restore(context); }
    const char* Label() const override { return "Delete Entity"; }
};

// Moving an entity to a different parent, or out to root level (parent INVALID_ENTITY).
// Restores the original sibling position, not just the original parent, so undoing a drag puts
// the row back where it was in the Hierarchy.
class ReparentCommand : public EditorCommand {
   public:
    ReparentCommand(CommandContext& context, Entity entity, Entity newParent);

    void Do(CommandContext& context) override;
    void Undo(CommandContext& context) override;
    const char* Label() const override { return "Reparent Entity"; }

   private:
    EntityRef entity_;
    EntityRef oldParent_;
    EntityRef newParent_;
    // Where it sat among oldParent_'s children; -1 when it had no parent.
    int oldIndex_ = -1;
};

// Reordering an entity among its siblings, or among root-level entities (where the order is
// the World's living-entity order, which is what the Hierarchy lists).
class ReorderCommand : public EditorCommand {
   public:
    // `before` picks which side of `sibling` the entity lands on.
    ReorderCommand(CommandContext& context, Entity entity, Entity sibling, bool before);

    void Do(CommandContext& context) override;
    void Undo(CommandContext& context) override;
    const char* Label() const override { return "Reorder Entity"; }

   private:
    void MoveBeside(CommandContext& context, EntityRef anchor, bool before) const;

    EntityRef entity_;
    EntityRef sibling_;
    bool before_ = true;
    // The entity this one followed before the move, so undo can put it back. Invalid when it
    // was already first.
    EntityRef oldPredecessor_;
};

}  // namespace Elysium

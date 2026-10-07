#include "Editor/Commands/EditorCommands.h"

#include <algorithm>
#include <utility>
#include "Core/Components/ParentComponent.h"
#include "Core/EntitySerializer.h"
#include "Core/World.h"
#include "Editor/EditorApplication.h"

namespace Elysium {

namespace {

// The ordered list `entity` is positioned within: its parent's children, or the world's
// living entities when it sits at root level (which is the order the Hierarchy lists roots in).
std::vector<Entity> SiblingsOf(World& world, Entity entity) {
    const Entity parent = world.GetParent(entity);
    if (parent != INVALID_ENTITY) return world.GetChildren(parent);
    return world.GetLivingEntities();
}

// The entity immediately ahead of `entity` among its siblings, or INVALID_ENTITY when it is
// already first.
Entity PredecessorOf(World& world, Entity entity) {
    const std::vector<Entity> siblings = SiblingsOf(world, entity);
    auto it = std::find(siblings.begin(), siblings.end(), entity);
    if (it == siblings.end() || it == siblings.begin()) return INVALID_ENTITY;
    return *(it - 1);
}

}  // namespace

namespace EditorOps {

void Reparent(World& world, Entity entity, Entity parent) {
    if (world.HasComponent<ParentComponent>(entity)) {
        auto& pc = world.GetComponent<ParentComponent>(entity);
        if (pc.parent != INVALID_ENTITY) world.RemoveChild(pc.parent, entity);
        if (parent == INVALID_ENTITY) {
            world.RemoveComponent<ParentComponent>(entity);
            return;
        }
        pc.parent = INVALID_ENTITY;
        pc.targetName.clear();  // AddChild fills it with the new parent's name
    }
    if (parent == INVALID_ENTITY) return;
    world.AddChild(parent, entity);
    // Living order too, not just the child list: the Hierarchy lists roots in world order and
    // the renderer draws in it, so a child that sorts before its parent would draw underneath.
    world.MoveEntityAfter(entity, parent);
}

void PlaceBeside(World& world, Entity entity, Entity anchor, bool before) {
    if (anchor == INVALID_ENTITY || anchor == entity) return;

    const Entity parent = world.GetParent(entity);
    if (parent != INVALID_ENTITY && world.GetParent(anchor) == parent) {
        if (before) {
            world.InsertChildBefore(parent, entity, anchor);
        } else {
            // "After the anchor" is "before whatever follows the anchor"; nothing following
            // means append, which AddChild does.
            const std::vector<Entity>& children = world.GetChildren(parent);
            auto it = std::find(children.begin(), children.end(), anchor);
            const bool hasNext = it != children.end() && (it + 1) != children.end();
            const Entity next = hasNext ? *(it + 1) : INVALID_ENTITY;
            if (next != INVALID_ENTITY && next != entity) {
                world.InsertChildBefore(parent, entity, next);
            } else {
                world.AddChild(parent, entity);
            }
        }
    }

    if (before) {
        world.MoveEntityBefore(entity, anchor);
    } else {
        world.MoveEntityAfter(entity, anchor);
    }
}

}  // namespace EditorOps

// --- ComponentEditCommand -----------------------------------------------------------------

ComponentEditCommand::ComponentEditCommand(EntityRef entity, std::string componentTag, std::string before,
                                           std::string after, std::string label)
    : entity_(entity),
      component_(std::move(componentTag)),
      before_(std::move(before)),
      after_(std::move(after)),
      label_(std::move(label)) {}

void ComponentEditCommand::Do(CommandContext& context) {
    const Entity entity = context.editor.EntityForStableId(entity_.id);
    if (entity == INVALID_ENTITY) return;
    EntityXml::LoadComponent(context.world, entity, component_, after_, context.services);
}

void ComponentEditCommand::Undo(CommandContext& context) {
    const Entity entity = context.editor.EntityForStableId(entity_.id);
    if (entity == INVALID_ENTITY) return;
    EntityXml::LoadComponent(context.world, entity, component_, before_, context.services);
}

bool ComponentEditCommand::MergeWith(const EditorCommand& newer) {
    const auto* other = dynamic_cast<const ComponentEditCommand*>(&newer);
    if (!other || other->entity_.id != entity_.id || other->component_ != component_) return false;
    // Keep our starting value, take their ending one: the whole gesture becomes one edit.
    after_ = other->after_;
    return true;
}

// --- SubtreeCommand -----------------------------------------------------------------------

void SubtreeCommand::Bind(CommandContext& context, Entity root) {
    root_.id = context.editor.StableIdOf(root);
    parent_.id = context.editor.StableIdOf(context.world.GetParent(root));
}

void SubtreeCommand::Capture(CommandContext& context, Entity root) {
    Bind(context, root);
    xml_ = EntityXml::SaveSubtree(context.world, root);

    stableIds_.clear();
    for (Entity entity : context.world.GetSubtree(root)) {
        stableIds_.push_back(context.editor.StableIdOf(entity));
    }
}

bool SubtreeCommand::Exists(CommandContext& context) const {
    return context.editor.EntityForStableId(root_.id) != INVALID_ENTITY;
}

void SubtreeCommand::Restore(CommandContext& context) {
    if (xml_.empty() || Exists(context)) return;

    // A parent that has itself been destroyed leaves the subtree at root level rather than
    // dropping it: losing the nesting is recoverable, losing the entities is not.
    const Entity parent = context.editor.EntityForStableId(parent_.id);

    std::vector<Entity> created;
    const Entity root = EntityXml::LoadSubtree(context.world, xml_, parent, context.services, &created);
    if (root == INVALID_ENTITY) return;

    // SaveSubtree and LoadSubtree walk in the same order, so the ids line up positionally.
    for (size_t i = 0; i < created.size() && i < stableIds_.size(); i++) {
        context.editor.RebindStableId(stableIds_[i], created[i]);
    }
}

void SubtreeCommand::Remove(CommandContext& context) {
    const Entity root = context.editor.EntityForStableId(root_.id);
    if (root == INVALID_ENTITY) return;

    // Children before parents, so a parent is never destroyed out from under its own subtree.
    const std::vector<Entity> subtree = context.world.GetSubtree(root);
    for (auto it = subtree.rbegin(); it != subtree.rend(); ++it) {
        context.world.DestroyEntity(*it);
    }
    for (uint64_t id : stableIds_) {
        context.editor.RebindStableId(id, INVALID_ENTITY);
    }
}

// --- SpawnCommand -------------------------------------------------------------------------

SpawnCommand::SpawnCommand(CommandContext& context, Entity created, std::string label)
    : label_(std::move(label)) {
    Bind(context, created);
}

void SpawnCommand::Undo(CommandContext& context) {
    const Entity root = context.editor.EntityForStableId(root_.id);
    if (root != INVALID_ENTITY) Capture(context, root);
    Remove(context);
}

// --- DeleteEntityCommand ------------------------------------------------------------------

DeleteEntityCommand::DeleteEntityCommand(CommandContext& context, Entity entity) {
    Capture(context, entity);
}

// --- ReparentCommand ----------------------------------------------------------------------

ReparentCommand::ReparentCommand(CommandContext& context, Entity entity, Entity newParent) {
    entity_.id = context.editor.StableIdOf(entity);
    newParent_.id = context.editor.StableIdOf(newParent);

    const Entity oldParent = context.world.GetParent(entity);
    oldParent_.id = context.editor.StableIdOf(oldParent);
    if (oldParent != INVALID_ENTITY) {
        const std::vector<Entity>& children = context.world.GetChildren(oldParent);
        auto it = std::find(children.begin(), children.end(), entity);
        if (it != children.end()) oldIndex_ = (int)(it - children.begin());
    }
}

void ReparentCommand::Do(CommandContext& context) {
    const Entity entity = context.editor.EntityForStableId(entity_.id);
    if (entity == INVALID_ENTITY) return;

    const Entity newParent = context.editor.EntityForStableId(newParent_.id);
    EditorOps::Reparent(context.world, entity, newParent);
}

void ReparentCommand::Undo(CommandContext& context) {
    const Entity entity = context.editor.EntityForStableId(entity_.id);
    if (entity == INVALID_ENTITY) return;

    const Entity oldParent = context.editor.EntityForStableId(oldParent_.id);
    EditorOps::Reparent(context.world, entity, oldParent);
    if (oldParent == INVALID_ENTITY || oldIndex_ < 0) return;

    // Reparent appends, so put it back at the index it came from.
    const std::vector<Entity>& children = context.world.GetChildren(oldParent);
    if (oldIndex_ < (int)children.size() && children[oldIndex_] != entity) {
        context.world.InsertChildBefore(oldParent, entity, children[oldIndex_]);
    }
}

// --- ReorderCommand -----------------------------------------------------------------------

ReorderCommand::ReorderCommand(CommandContext& context, Entity entity, Entity sibling, bool before)
    : before_(before) {
    entity_.id = context.editor.StableIdOf(entity);
    sibling_.id = context.editor.StableIdOf(sibling);
    oldPredecessor_.id = context.editor.StableIdOf(PredecessorOf(context.world, entity));
}

void ReorderCommand::MoveBeside(CommandContext& context, EntityRef anchor, bool before) const {
    const Entity entity = context.editor.EntityForStableId(entity_.id);
    if (entity == INVALID_ENTITY) return;

    Entity target = context.editor.EntityForStableId(anchor.id);
    if (target == INVALID_ENTITY) {
        // No anchor means "first": anchor on whatever currently leads the sibling list.
        const std::vector<Entity> siblings = SiblingsOf(context.world, entity);
        if (siblings.empty() || siblings.front() == entity) return;
        EditorOps::PlaceBeside(context.world, entity, siblings.front(), true);
        return;
    }
    EditorOps::PlaceBeside(context.world, entity, target, before);
}

void ReorderCommand::Do(CommandContext& context) {
    MoveBeside(context, sibling_, before_);
}

void ReorderCommand::Undo(CommandContext& context) {
    // Back to following whatever it followed before; an invalid predecessor means it was first.
    MoveBeside(context, oldPredecessor_, false);
}

}  // namespace Elysium

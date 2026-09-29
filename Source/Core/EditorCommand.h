#pragma once

#include <cstdint>

namespace Elysium {

class ServiceLocator;
class World;

namespace Services {
class IEditorService;
}

// A handle to an entity that stays meaningful across undo cycles, unlike `Entity` itself
// (a recycled index with no generation counter). Resolve it through
// IEditorService::EntityForStableId every time you need the live id; never cache the result,
// since an undo can rebind it to a newly created entity.
struct EntityRef {
    uint64_t id = 0;
    bool Valid() const { return id != 0; }
};

// What a command is allowed to touch. Commands are stored in a CommandHistory that outlives
// any particular frame, so they must never capture a World* or a component reference — those
// go stale when a document is reloaded or an entity is destroyed and recreated. They capture
// data (entity references and serialized component XML) and are handed the world to act on.
struct CommandContext {
    World& world;
    Services::IEditorService& editor;
    ServiceLocator& services;
};

// One undoable editor mutation.
//
// Do() must be idempotent: the history calls it once when the command is first executed, and
// again on every redo. Several call sites (the gizmo, the Inspector) let the widget write the
// live value first and then record what changed, so the first Do() is often re-applying a
// value that is already there.
//
// Undo() must return the world to the state Do() found it in. A command that cannot express
// its own inverse cheaply should not exist -- use a SnapshotCommand instead.
class EditorCommand {
   public:
    virtual ~EditorCommand() = default;

    virtual void Do(CommandContext& context) = 0;
    virtual void Undo(CommandContext& context) = 0;

    // Shown in Edit > Undo / Redo.
    virtual const char* Label() const = 0;

    // Folds a continuing gesture into this already-stored command, so a drag that produces a
    // command every frame is still one undo step. `newer` has already run its Do(); absorbing
    // it means taking its end state while keeping this command's start state. Return false to
    // let it be recorded separately.
    //
    // Only consulted between BeginGesture/EndGesture, so a command never merges with an
    // unrelated earlier edit that happens to sit on top of the stack.
    virtual bool MergeWith(const EditorCommand& newer) { return false; }
};

}  // namespace Elysium

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Editor/Commands/EditorCommand.h"

namespace Elysium {

// Undo/redo for one editor document. Every editor mutation is expected to arrive here through
// EditorApplication::Execute rather than reaching into the World, which is what makes the whole
// history trustworthy: an operation that bypasses it doesn't just miss undo, it leaves the
// stack describing a world that no longer exists.
//
// Two ways several commands collapse into one undo step:
//
//   Transactions -- an explicit Begin/End pair around a batch the user thinks of as a single
//                   action: a paint stroke across forty cells, or one Inspector field applied
//                   across a multiselect. The commands stay distinct and are undone in reverse.
//
//   Gestures     -- a Begin/End pair around a continuous drag, where every frame produces
//                   another command for the same thing. A gesture is a transaction that also
//                   lets a new command be absorbed into a matching one already in the group
//                   (EditorCommand::MergeWith), so a drag is one step however long it lasted
//                   and however many entities it moved.
class CommandHistory {
   public:
    // Beyond this the oldest entries are dropped. Undo depth is a convenience, not a
    // guarantee -- holding every edit of a long session costs memory for a history nobody
    // walks back that far.
    static constexpr size_t MaxDepth = 128;

    // Runs the command, then records it. Clears the redo stack, since redoing would replay
    // commands whose starting state no longer exists.
    void Execute(CommandContext& context, std::unique_ptr<EditorCommand> command);

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }
    // Null when there is nothing to undo/redo, so a menu item can use it directly.
    const char* UndoLabel() const { return undo_.empty() ? nullptr : undo_.back().label.c_str(); }
    const char* RedoLabel() const { return redo_.empty() ? nullptr : redo_.back().label.c_str(); }

    void Undo(CommandContext& context);
    void Redo(CommandContext& context);

    // Nestable, so a caller can wrap a batch without knowing whether its own caller already
    // did. Only the outermost pair produces an undo entry.
    void BeginTransaction(const std::string& label);
    void EndTransaction();
    bool InTransaction() const { return transactionDepth_ > 0; }

    // A gesture is a transaction with merging enabled; Begin/End nest the same way. Ending a
    // gesture that recorded nothing (a click that moved nothing) leaves no entry behind.
    void BeginGesture(const std::string& label);
    void EndGesture();
    bool InGesture() const { return gestureDepth_ > 0; }

    void Clear();

    // Steps executed since the last MarkSaved, negative after undoing back past it. The
    // viewport tab shows unsaved state from this rather than from a bool, so undoing every
    // change since a save correctly reads as clean again.
    int DirtySteps() const { return dirtySteps_; }
    bool IsDirty() const { return dirtySteps_ != 0; }
    void MarkSaved() { dirtySteps_ = 0; }

   private:
    // One undo step. A lone command is a group of one, so the stacks only ever hold groups
    // and Undo/Redo need no special case.
    struct Group {
        std::string label;
        std::vector<std::unique_ptr<EditorCommand>> commands;
    };

    void Push(Group group);

    std::vector<Group> undo_;
    std::vector<Group> redo_;

    // The transaction or gesture currently accumulating, instead of the undo stack.
    Group open_;
    int transactionDepth_ = 0;
    int gestureDepth_ = 0;
    int dirtySteps_ = 0;
};

}  // namespace Elysium

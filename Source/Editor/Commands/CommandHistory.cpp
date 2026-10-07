#include "Editor/Commands/CommandHistory.h"

#include <utility>

namespace Elysium {

void CommandHistory::Execute(CommandContext& context, std::unique_ptr<EditorCommand> command) {
    if (!command) return;

    command->Do(context);

    if (transactionDepth_ > 0) {
        // Inside a gesture, try to fold this into the matching command already in the group.
        // Scanning rather than only checking the last one matters for a multiselect drag: a
        // frame produces one command per selected entity, so the match for this entity is
        // several entries back.
        if (gestureDepth_ > 0) {
            for (auto& existing : open_.commands) {
                if (existing->MergeWith(*command)) return;
            }
        }
        open_.commands.push_back(std::move(command));
        return;
    }

    Group group;
    group.label = command->Label();
    group.commands.push_back(std::move(command));
    Push(std::move(group));
}

void CommandHistory::Push(Group group) {
    if (group.commands.empty()) return;

    undo_.push_back(std::move(group));
    redo_.clear();
    dirtySteps_++;

    if (undo_.size() > MaxDepth) {
        undo_.erase(undo_.begin(), undo_.begin() + (undo_.size() - MaxDepth));
    }
}

void CommandHistory::Undo(CommandContext& context) {
    if (undo_.empty()) return;

    Group group = std::move(undo_.back());
    undo_.pop_back();

    // Reverse order: a group's later commands may depend on what its earlier ones created.
    for (auto it = group.commands.rbegin(); it != group.commands.rend(); ++it) {
        (*it)->Undo(context);
    }

    redo_.push_back(std::move(group));
    dirtySteps_--;
}

void CommandHistory::Redo(CommandContext& context) {
    if (redo_.empty()) return;

    Group group = std::move(redo_.back());
    redo_.pop_back();

    for (auto& command : group.commands) {
        command->Do(context);
    }

    undo_.push_back(std::move(group));
    dirtySteps_++;
}

void CommandHistory::BeginTransaction(const std::string& label) {
    if (transactionDepth_++ == 0) {
        open_ = Group{};
        open_.label = label;
    }
}

void CommandHistory::EndTransaction() {
    if (transactionDepth_ == 0) return;
    if (--transactionDepth_ > 0) return;

    // An empty group (a click that changed nothing) leaves no entry to undo.
    Push(std::move(open_));
    open_ = Group{};
}

void CommandHistory::BeginGesture(const std::string& label) {
    gestureDepth_++;
    BeginTransaction(label);
}

void CommandHistory::EndGesture() {
    if (gestureDepth_ > 0) gestureDepth_--;
    EndTransaction();
}

void CommandHistory::Clear() {
    undo_.clear();
    redo_.clear();
    open_ = Group{};
    transactionDepth_ = 0;
    gestureDepth_ = 0;
    dirtySteps_ = 0;
}

}  // namespace Elysium

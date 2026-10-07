---
description: Branch, commit, push and open a PR for the current work
argument-hint: "[optional note for the PR or branch name]"
---

Ship the work from this session as a pull request. Extra instructions from the user: $ARGUMENTS

1. Run `git status` and `git diff` to see what changed. Decide which files belong to this task. If there are changes you didn't make or can't place, list them and ask before going on.
2. If the current branch is `main`, create `feature/<short-slug>` with `git switch -c` as its own command. Otherwise stay on the current branch.
3. Stage the task's files by name and commit. The subject follows the history's style: one line, third-person present, sentence case, no period (e.g. "Adds portrait art to characters"). Add a short body only when the why isn't obvious. End the message with the attribution trailer from the system reminder.
4. Push with `git push -u origin <branch>`.
5. Open the PR with `gh pr create --base main`. The title is the commit subject, or a summary if the branch has several commits. The body has three short sections:
   - **What**: the change, in a few bullets.
   - **Why**: the reason, in a sentence or two.
   - **To test**: the scene to open and what to click or watch. Note anything unverified, e.g. Lua or XML changes that weren't run.
6. Reply with the PR link and stop. Never merge; the user reviews and merges on GitHub.

If the git guard hook blocks a step, don't work around it. Tell the user what was blocked.

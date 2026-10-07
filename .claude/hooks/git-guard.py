# PreToolUse hook: refuses git commands that are hard to undo or that skip review.
import json, re, subprocess, sys

command = json.load(sys.stdin).get("tool_input", {}).get("command", "")
if not re.search(r"\b(git|gh)\b", command):
    sys.exit(0)

rules = [
    (r"git\s+push\b.*(--force|--force-with-lease|\s-f\b|\s\+\S)", "force-pushing"),
    (r"git\s+push\b.*\bmain\b", "pushing to main"),
    (r"git\s+reset\b.*--hard", "git reset --hard"),
    (r"git\s+branch\b.*\s-D\b", "force-deleting a branch"),
    (r"git\s+clean\b", "git clean"),
    (r"git\s+(checkout|restore)\s+(--\s+)?\.(\s|$)", "discarding all working-tree changes"),
    (r"git\s+add\s+(-A|--all|\.)(\s|$)", "staging everything; add the task's files by name"),
    (r"git\s+commit\b.*--amend", "amending a commit"),
    (r"git\s+(merge|rebase)\b", "merging or rebasing; the user merges on GitHub"),
    (r"gh\s+pr\s+merge\b", "merging a PR; the user merges on GitHub"),
    (r"--no-verify", "skipping hooks"),
]
for pattern, what in rules:
    if re.search(pattern, command):
        print(f"Blocked by git guard: {what}. Ask the user to do this themselves if it's really needed.", file=sys.stderr)
        sys.exit(2)

if re.search(r"git\s+(commit|push)\b", command):
    branch = subprocess.run(["git", "branch", "--show-current"], capture_output=True, text=True).stdout.strip()
    # Branch creation must run as its own command first, so the check sees the new branch.
    if branch == "main":
        print("Blocked by git guard: committing or pushing on main. Create a feature/<slug> branch first, as a separate command.", file=sys.stderr)
        sys.exit(2)

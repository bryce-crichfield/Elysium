#!/usr/bin/env bash
# Stop hook: when Claude finishes a turn with engine sources changed since the last good build,
# build incrementally and send any errors back to Claude instead of letting it stop.
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
[ -f Build/CMakeCache.txt ] || exit 0  # never configured; leave the first build to elysium.ps1

paths=(Source CMakeLists.txt)
fingerprint=$( {
    git diff HEAD -- "${paths[@]}"
    git ls-files --others --exclude-standard -- "${paths[@]}" | while read -r f; do echo "$f"; cat "$f"; done
} | md5sum | cut -d' ' -f1)

stamp=Build/.claude-build-ok
failures=Build/.claude-build-failures
[ "$(cat "$stamp" 2>/dev/null)" = "$fingerprint" ] && exit 0

export PATH="/c/msys64/mingw64/bin:$PATH"
if output=$(cmake -B Build >/dev/null 2>&1 && cmake --build Build 2>&1); then
    echo "$fingerprint" > "$stamp"
    rm -f "$failures"
    exit 0
fi

# Give up after three failed attempts in a row so a stuck fix can't loop forever.
count=$(( $(cat "$failures" 2>/dev/null || echo 0) + 1 ))
echo "$count" > "$failures"
if [ "$count" -gt 3 ]; then
    rm -f "$failures"
    exit 0
fi

{
    echo "Build failed (attempt $count of 3). Fix these before finishing:"
    echo "$output" | grep -E 'error|Error|undefined reference' | head -40
    echo "--- tail of build output ---"
    echo "$output" | tail -15
} >&2
exit 2

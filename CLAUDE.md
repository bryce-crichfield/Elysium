# CLAUDE.md

Elysium is a C++20 engine for isometric 3D games: ECS core, raylib behind a mirror-type seam, Lua via sol2, XML scenes/prefabs/assets, an ImGui in-engine editor, ENet networking. One executable (`Elysium`) runs a Project (`Projects/<Name>/Project.xml`) as a game, or with `--Editor` as its editor. The active game is **`Projects/Battler`** (a turn-based tactics POC: Town ↔ Battle encounters); `DemoGame` and `HelloWorld` are older samples.

## How we work

The user prompts, reviews and merges. These rules hold unless the user says otherwise in the moment.

- **Do the task asked, then stop.** If the user says they'll do something themselves, explain how; don't do it. Ask before widening scope (new systems, refactors you noticed on the way).
- **Report honestly.** Say what was built and verified, and what wasn't (Lua and XML changes can't be checked without running the game; say so). Don't claim something works because it compiles.
- **Shelved work stays shelved.** Enemy intent telegraphing was tried and reverted; don't reintroduce it unprompted.

### Verify before reporting done

- A Stop hook (`.claude/hooks/build-check.sh`) builds incrementally whenever a turn ends with `Source/` or `CMakeLists.txt` changed since the last good build, and hands the errors back if it fails. Fix them rather than reporting done; after three failed attempts it lets the turn end, so say plainly that the build is broken. Never `--Clean` without asking; a clean rebuild is slow.
- Don't launch the game or editor; it's interactive and the user playtests. End with a short **"To test"** list: the scene to open and what to click or watch.
- Lua (`Projects/*/Scripts`) has no checker here. Re-read the edited functions for nil access, misspelled fields and stale callers before reporting.
- The network tests (`pytest Tests/Network/network_tests.py`) need a running server; only run them when touching networking, and ask first.

### Git

- Don't commit, push or open PRs on your own. The user runs `/ship` (`.claude/commands/ship.md`) when work is ready; it branches off `main`, commits the task's files, pushes and opens a PR.
- A guard hook (`.claude/hooks/git-guard.py`) refuses commits or pushes on `main`, force pushes, `reset --hard`, `git add -A`/`.`, amends, merges and rebases. If it blocks something, tell the user; don't work around it.

## Build / run / test

MSYS2/MinGW + Ninja via CMake, driven by `elysium.ps1` (`elysium.sh` on Linux/Mac, flags lowercase):

```powershell
.\elysium.ps1 --Build                                # incremental Debug build
.\elysium.ps1 --Clean --Build                        # wipe Build/ and Binary/, rebuild
.\elysium.ps1 --Build --Mode=Release --Tracy         # -O2 build with the profiler
.\elysium.ps1 --Run --Project=Projects\Battler [--Editor]
```

- Operations run Clean → Build → Run regardless of flag order. `--Build` forces `-DTRACY_ENABLE=OFF` (Tracy's threads corrupt the stack in this static MinGW build; don't flip it without re-checking).
- `Build/` holds intermediates and `compile_commands.json`; `Binary/` is the runtime dir (`Elysium.exe` + copied assets) and asset paths resolve from it.
- New `.cpp` files under `Source/` are globbed (no CMake edit), but need a `--Build` to re-configure before Ninja sees them.
- There is no C++ unit test suite. `Tests/Network` holds pytest integration tests against a running server (port 7777).

### Network protocol codegen

`Source/Core/Generated.h` (C++) and `Tools/elysium/elysium/generated.py` (Python) are both generated from `Tools/eidc/Invoke.xml` by `Tools/eidc/eidc.py` — **never hand-edit either generated file**; edit the IDL and regenerate. Codegen is manual (not part of the build), and the IDL is parked in `Tools/` until networking is reworked so games can define their own protocol:

```
python Tools/eidc/eidc.py Tools/eidc/Invoke.xml
```

## Code style

- **Comments are light.** Most code should read on its own through its names. Leave a comment only where the *why* isn't obvious from the code: a non-obvious constraint, a workaround, an ordering that matters. No comments that restate the code, narrate a change ("now does X", "moved from Y"), or label every block. A short line on a declaration in a header is fine when the name alone doesn't say enough; paragraphs aren't.
- **Fit the existing design; don't bolt on.** A new feature should slot into the flow that's already there (the service's queue, the asset pipeline, the registry) rather than poking flags and special-case conditionals into existing code. If it needs lots of `if (newThing)` checks scattered around, the design is wrong.
- **Keep responsibilities where they belong.** Policy lives in the service that owns it; Core types expose plain state (e.g. `Scene::IsSetUp()`) instead of reaching into services to decide things themselves.
- **No speculative API.** Build what's used now. Don't add fields, options or overloads "in case we need them later"; they're cheap to add when they're needed.
- **Delete dead code.** When something has no callers (old POC paths, unused messages), remove it and its includes instead of leaving it around.
- **Names are descriptive and idiomatic to their surroundings.** A type called `Loading` is too vague; `LoadingJob` says what it is. Lua bindings follow the existing families (`SceneReplace`, `ScenePush`, `SceneLoading`): noun-first, no `Get` prefix. C++ getters keep `Get`.
- **Headers are ordered semantically.** Group declarations by topic (lifecycle, operations, queries, then each feature), with a short section comment; in the private section, types first, then helpers, then members, grouped the same way.
- **Never block the main thread on assets.** Assets load async through `IAssetService`; anything a scene needs goes in its preloads. A component whose asset isn't in yet no-ops instead of failing.
- **Match the surrounding code.** Same idioms, naming and comment density as the file you're in.


## Architecture

[ARCHITECTURE.md](ARCHITECTURE.md) is the tour of the engine: services, ECS, scenes/prefabs/assets, rendering, the 3D world, the editor, scripting, networking. Read the relevant section before changing a subsystem you haven't touched this session.

Traps worth knowing even without the tour:

- **ODR hazard.** In a header inside `namespace Elysium`, a bare `Color`/`Rectangle`/`Vector2`/`Texture` silently resolves to either the mirror type or raylib's, depending on include order. Write `::Color` or `Elysium::Color`. `RaylibConvert.h` is only ever included from a `.cpp`.
- **Script loading.** `ScriptComponent::LoadXml` must go through `IAssetService::LoadAsset`; `ScriptSystem` gates init on its return value, so bypassing it silently skips the script.
- **Editor and game never share scene state.** The editor edits its own document scenes; Play reloads saved files from disk. Nothing outside `Editor/` includes `Editor/` or checks for editor mode.
- **Generated files.** Never hand-edit `Source/Core/Generated.h` or `generated.py`; edit `Tools/eidc/Invoke.xml` and regenerate.

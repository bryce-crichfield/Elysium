# Prefab System Redesign — Plan

Status: design agreed, not yet implemented. Picking this up later.

## Why

The current prefab mechanism (`<Include src="..." key="value".../>` in scene XML,
processed by `ProcessIncludes` in `Source/Core/Xml.cpp`) was the first idea, not a
considered design. It works like this today:

1. `ProcessIncludes` does raw text `$key` → `value` substitution on the prefab
   file's contents, parses the result, and deep-clones the resulting `<Entities>`
   block directly into the scene document — before any `World` exists.
2. Once loaded, the spawned entities are indistinguishable from hand-authored
   scene entities. Nothing records that they came from a prefab.
3. `SaveEntities` (`Source/Core/SceneSaver.cpp`) walks every living entity and
   flattens it back to a plain `<Entity>` block. The `<Include>` tag never comes
   back once a scene is saved.

This has two concrete problems:
- **No editor workflow**: there's no way to open a prefab file on its own and
  edit it — the editor (`WorldEditor`) only ever edits the live scene's `World`
  (`EditorService::GetWorld()` is hardwired to `sceneService.GetTopScene()`).
- **No propagation**: editing a prefab file does nothing for scenes that already
  placed a copy of it, because nothing links a placed entity back to its source.

Decision: we DO want propagation (edit a prefab, see it reflected in scenes that
reference it — this is important to the intended workflow of building levels
from prefabs and iterating on them). Given that, the `$key` text-substitution
model is the wrong foundation — it can't be diffed. Prefabs need structured
default values instead.

## Non-goals

- No requirement for *live* hot-reload while the game is running mid-edit;
  propagation happens on next scene load (and optionally on-demand refresh in
  the editor), not via a file watcher.
- Not building a generic reflection/override system independent of XML — we
  reuse the existing per-component XML load/save round-trip as the diffing
  substrate (see below), not a new mechanism.

## Data model

### Prefab file format

A prefab file becomes a plain mini-scene: normal `<Entity>` blocks with real,
authored default values — no `$name`/`$x` tokens.

```xml
<Entities>
    <Entity id="0">
        <NameComponent name="Unit" />
        <TransformComponent x="0" y="0" />
        <MovementComponent state="Idle" goalX="0" goalY="0" />
        <KinematicsComponent maxSpeed="150" friction="5" />
        <SpriteComponent spriteName="Sprites/Archer/Archer.xml" sheetName="Idle" sequenceName="south" />
        <ColliderComponent width="32" height="32" />
        <HealthComponent max="100" />
        <TeamComponent team="0" />
        <ScriptComponent scriptName="Scripts/Elysium/Unit.lua" />
        <LayerComponent name="entity" />
    </Entity>
    <Entity id="1">
        <ParentComponent target="0" />  <!-- see naming section -->
        <FollowComponent followSpeed="0" />
        <TransformComponent />
        <LayerComponent name="shadow" />
        <LightComponent color="#ffffffff" radius="500" intensity="0.0" />
        <ColliderComponent width="600" height="300" />
    </Entity>
</Entities>
```

- `id` is a stable local identifier within the prefab file (author-assigned or
  defaulted to file order on first save from the editor). Used to address
  overrides and to resolve intra-prefab parenting.
- Existing prefab files (`Projects/DemoGame/Scenes/Prefabs/*.xml`) need a
  one-time migration pass: replace `$key` tokens with concrete default values
  and add `id` attributes.

### Scene XML format

Replace `<Include>` with `<PrefabInstance>` + `<Override>`:

```xml
<PrefabInstance src="Prefabs/Unit.xml" id="Knight1">
    <Override entity="0" component="TransformComponent" field="x" value="320" />
    <Override entity="0" component="TransformComponent" field="y" value="480" />
    <Override entity="0" component="NameComponent" field="name" value="Knight1" />
</PrefabInstance>
```

- `id` on `<PrefabInstance>` is the placement's unique instance id within the
  scene (used for naming — see below — and as the key overrides are computed
  against on save).
- Only fields that differ from the prefab's current defaults are written. This
  is what makes propagation work: anything not listed as an `Override` is read
  fresh from the prefab file every load, so prefab edits show up automatically.

### Instance naming

Today `$name` exists only so multi-entity prefabs can resolve internal
parent/child links without name collisions across placements (see
`Unit.xml`'s shadow entity: `<ParentComponent target="$name" />`). Without
params, the loader auto-namespaces entity names per instance instead:
spawned names become `"<instanceId>::<localName-or-id>"` (e.g.
`"Knight1::Unit"`, `"Knight1::Shadow"`), and intra-prefab `ParentComponent`
targets referencing a local `id` are resolved the same way. This is more
general than `$name` and needs no declared param schema.

## Runtime load path (`Source/Core/SceneLoader.cpp`, `Source/Core/Xml.cpp`)

Replace `ProcessIncludes` with a load-time step that, for each `<PrefabInstance>`:

1. Loads and parses the referenced prefab file fresh (small file, cheap; no
   need to cache yet).
2. Spawns each `<Entity>` in the prefab into the scene's `World`, namespacing
   names per the scheme above.
3. Tags every spawned entity with a lightweight `PrefabInstanceComponent
   { src, instanceId, localEntityId }` — needed at save time to know which
   entities belong to which instance and to re-derive `id`/defaults.
4. Applies the `<Override>` list on top, using the same per-component
   `XMLLoadable`/`ComponentRegistry` machinery already used for normal entity
   loading (an override just becomes "load this one field via the existing
   component XML loader after the entity is otherwise fully loaded").
5. Runs `ResolveHierarchy` as today, after all instances (and hand-authored
   entities) exist, so both intra-prefab and cross-prefab parenting resolve
   uniformly.

## Save path (`Source/Core/SceneSaver.cpp`)

For entities carrying `PrefabInstanceComponent`:

1. Group by `instanceId`.
2. Re-parse the referenced prefab file's current defaults (same file used at
   load).
3. For each component on each entity, serialize the live component to an XML
   fragment using the existing `ComponentRegistry` `XmlSaver`, and do the same
   for the matching default entity/component from the freshly-parsed prefab.
   Diff attribute-by-attribute; only mismatches become `<Override>` entries.
4. Emit `<PrefabInstance src=... id=...>` with just those overrides — not the
   full flattened entity.
5. Entities without `PrefabInstanceComponent` continue to save exactly as
   today (plain `<Entity>` blocks).

This reuses existing per-component load/save code as the diff substrate —
no new generic reflection system required.

## Edge cases

- **Prefab entity removed from source file, but a scene instance still has
  overrides addressing its local `id`**: on load, log a warning and drop the
  dangling overrides for that entity id; don't fail the whole instance.
- **Prefab entity added to source file after instances already exist**: shows
  up automatically in every instance next load (no override references it, so
  it's just spawned with defaults) — this is a desired propagation behavior,
  not an edge case to guard against.
- **Missing prefab file at load time**: skip the `PrefabInstance` block, log
  an error (matches current `Include` behavior for missing files).

## Editor workflow

### Editing a prefab file directly

- Add `AssetType::PREFAB` (`Source/Core/Asset.h`) so prefab files show up
  distinctly in the Asset Browser (already scoped to project-only assets).
- `EditorService::GetWorld()` (`Source/Services/EditorService.cpp:51`) is
  hardwired to `sceneService.GetTopScene()->GetWorld()`. Give it an overridable
  target: when "editing a prefab," create a scratch `Scene`/`World`, load just
  that prefab file's entities into it (reusing `LoadEntities`), and point
  `GetWorld()` at the scratch world instead. `WorldEditor` needs no changes —
  it only ever calls `EditorService::GetWorld()`.
- Saving while in this mode reuses `SaveEntities`, writing the scratch world's
  entities back to the prefab file as plain `<Entity id="...">` blocks (no
  `PrefabInstance`/`Override` involved — you're editing the source of truth).

### Placing a prefab instance into a scene

- An "Instantiate Prefab" editor action creates a new `PrefabInstance` in the
  live scene's `World` (steps 1-4 of the runtime load path above, minus
  overrides), selects the new entity/entities in `WorldEditor`, and lets the
  user edit them normally — edits become `Override` entries automatically at
  next save via the diffing described above.

## Suggested implementation order

1. `PrefabInstanceComponent` + `ComponentRegistry` diff helper (reusable XML
   fragment compare) — the load-bearing piece everything else depends on.
2. Scene load/save rewrite: `<PrefabInstance>`/`<Override>` replacing
   `<Include>`, using the diff helper.
3. Migrate existing prefabs (`Projects/DemoGame/Scenes/Prefabs/*.xml`) off
   `$key` tokens to real defaults + `id` attributes; migrate any scene XML
   using `<Include>` to `<PrefabInstance>`.
4. `AssetType::PREFAB` + Asset Browser recognition.
5. Editor: overridable `EditorService::GetWorld()` target + open-prefab-for-
   editing flow (scratch `Scene`/`World`).
6. Editor: "Instantiate Prefab" action for placing instances into a live
   scene.

## Open questions to revisit

- Should prefab-default re-parsing at load/save be cached (keyed by file path
  + mtime) once there are many instances per scene, or is per-instance
  re-parsing cheap enough to ignore for now?
- Do we want an explicit "detach from prefab" editor action (strip
  `PrefabInstanceComponent`, keep current values, save as plain entity) for
  cases where an instance needs to diverge structurally (extra component,
  etc.) beyond field-level overrides?

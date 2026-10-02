# POC — Occluders, Colliders, NavMesh (tile-free scenes)

> **2026-10-02:** OccluderComponent and OcclusionSystem were removed. World3D layers occlude with real geometry (depth buffer, model shadow maps, the camera-ray fade), and VisibilitySystem now ray-casts the models. The occluder sections below are history.

Branch: `feature/materials`. Status: implemented, every touched translation unit syntax-checked with `g++ -std=c++20 -Wall` against this branch's headers; not yet run on Windows. Try it: build, start ExploreScene, press **P**.

## The three layers

| Layer | Scope | Owner | Answers | Feeds |
|---|---|---|---|---|
| **Occluder** | per entity, *every* placed thing | `OccluderComponent` + `OcclusionSystem` | "does this thing's picture cover that thing?" | a `uTint` override on Texture material layers |
| **Collider** | per entity, only things that physically block | `ColliderComponent` (AABB path unchanged; optional polygon) | kinematics: overlap, push-out | `CollisionSystem`, `PhysicsResponseSystem`, navmesh bake |
| **NavMesh** | one per scene | `NavMeshSystem` (+ `NavAreaComponent` on prefabs) | "can an agent stand here / how do I get there?" | `MovementSystem`, Lua `NavIsWalkable` / `NavFindPath` |

Independent on purpose: a rug occludes nothing, blocks nothing, is walkable; a pit blocks pathing with no collider; a glass wall collides but occludes with `mode="None"`.

### Occluder

The *visual volume* a picture covers: a ground **footprint** polygon (local to the anchor) extruded up by **height** px. `OcclusionSystem::IsBehind(A, B)`:

1. B draws on top of A — `B.worldY > A.worldY`, the key `RenderSorter` Y-sorts on, and
2. A's sprite column (anchor up `height`) pokes into B's extruded volume (convex hull of footprint ∪ footprint − (0, height)).

Only **dynamic** occluders (`isStatic="false"`, characters) can be occluded; any occluder can occlude. `mode` per occluder (exposed as a prefab `Parameter` on the walls, so each placement picks):

- `FadeSelf` — the occluder's Texture layers get `uTint = (1,1,1,fadeAlpha)`.
- `TintOccluded` — the walker's Texture layers get `uTint = tint`.
- `None` — participates, never changes appearance (floors, characters).

The tint is written to `MaterialComponent.layers[i].overrides["uTint"]` on the entity **and its descendants** (a prefab's picture is often a child, e.g. Brazer). Whatever override was there before is remembered and put back the frame the pair separates, so authored tints survive. The system is not `RunsWhenPaused`, so an editor save can never persist a faded wall.

Footprint fallback: authored → collider polygon/AABB → 32×16 diamond. A dynamic with `height="0"` uses the tallest `RectangleComponent` in its subtree × scale (i.e. its sprite). Shorthand: `diamondW`/`diamondH` on load.

### Collider

Unchanged semantics. Added: optional `points="x,y x,y ..."` polygon (or `diamondW/H`); width/height/offset are derived from its bounds so the AABB path and gizmos keep working, while the navmesh bake uses the exact polygon. Added `SaveXml` — required for prefab diffing (the registry only builds `PrefabFieldSupport` for components that both load and save), and previously colliders were silently dropped on scene save.

Static = collider, no `KinematicsComponent`, not a trigger. Only statics are carved from the navmesh.

### NavMesh

Scene-level, Recast/Unity style: build the scene from prefabs, then it bakes from what's there. Per cell (`cellSize` px, orthogonal grid — iso shapes come from the polygons):

1. **Region** — everything the scene's static geometry spans, plus `padding` (so rooms are enclosed by colliders); or, if the scene has any painted `Walkable` areas, exactly those polygons.
2. Static colliders inflated by `agentRadius` subtract.
3. Painted `Blocked` areas subtract (pit, water); painted `Cost` areas scale A* traversal (mud, stairs).

Painted areas are plain scene entities with a `NavAreaComponent` (`Walkable` | `Blocked` | `Cost`), never part of a prefab — prefabs describe themselves with colliders and the bake reads those. Floors are art.

Rebakes automatically when the static signature changes (prefab placed, dragged in the editor, deleted, an area painted or edited, a tunable changed). A* is 8-connected without corner cutting, then string-pulled with line-of-sight. Goals off-mesh snap to the nearest walkable cell. `ClampMovers()` snaps any `Movement`+`Kinematics` entity that ends a frame off-mesh back to its last walkable position — that is what stops units walking through prefab walls now that there are no per-tile colliders.

Tunables are **system parameters**: `<System type="NavMeshSystem" cellSize agentRadius padding debugDraw/>`, editable in the Scene Editor's Systems tab. `MovementSystem` prefers `NavMeshSystem` and falls back to `SpatialSystem` for tilemap scenes.

#### Editing it in the viewport

Scene documents get a **Navmesh mode** toggle (route icon) on the viewport toolbar:

- Baked cells are drawn over the scene (green walkable, amber under a Cost area, grey box = bake bounds) and rebake live as you move prefabs or edit `cellSize` / `agentRadius` / `padding` in the Systems tab.
- Brushes: **Walkable region**, **Blocked**, **Cost**. Click to lay vertices; right-click, Enter or the first vertex closes; Esc cancels. Closing creates a `NavArea <Type>` entity at the centroid.
- No brush: click inside an area to select it, drag its vertex handles, move it with the gizmo, Delete removes it.

The **Overlays** dropdown (layers icon) toggles Colliders / Occluders / Nav areas for every entity; selected entities always show all of theirs plus a cross at the **anchor**, the ground point Y-sorting keys on. There is no play-mode debug drawing for these; the `debugDraw` parameter on `NavMeshSystem` only draws live paths.

## Files

New

- `Source/Components/OccluderComponent.{h,cpp}`, `NavAreaComponent.{h,cpp}`
- `Source/Systems/OcclusionSystem.{h,cpp}` (also `ResolveOccluder` / `IsBehind`, shared with the editor), `NavMeshSystem.{h,cpp}`
- `Source/Editor/OverlayPainter.{h,cpp}` — world-space drawing onto the viewport draw list
- `Source/Editor/SpatialOverlays.{h,cpp}` — collider / occluder / nav-area overlays and the Overlays menu
- `Source/Editor/PolygonHandles.{h,cpp}` — vertex-drag editing, reusable for collider and occluder polygons
- `Source/Editor/NavMeshTool.{h,cpp}` — navmesh mode
- `Projects/DemoGame/Prefabs/{Floor,Stairs,Pit,Wall_NE,Wall_NW,Pillar,Knight}.xml`, `Scenes/PrefabScene.xml`, `Scripts/Scenes/PrefabScene.lua`, `Textures/Prefabs/*.png` (placeholders)

Modified

- `Core/Geometry.{h,cpp}` — hull, bounds, distance, point-list parse/format, `IsoDiamond`
- `Core/Xml.{h,cpp}` — `ReadPolygonAttribute` (shared by the three polygon-carrying components)
- `Components/ColliderComponent.{h,cpp}` — `points`, `GetPolygon`, `SyncBoxToPolygon`, `SaveXml`
- `Components/MovementComponent.{h,cpp}` — `SaveXml`
- `Systems/MovementSystem.{h,cpp}` — navmesh preference, speed-scaled arrival, look-ahead, tolerant `IssueMoveCommand`
- `Services/ScriptService.cpp` — `NavIsWalkable`, `NavFindPath`, `NavSetDebugDraw`
- `Editor/ViewportEditor.{h,cpp}` — Overlays menu, navmesh mode wiring
- `Editor/CodePane.cpp` — `<cstdint>` for GCC 16; `Core/Components.h` — includes; `Scripts/Scenes/ExploreScene.lua` — **P** opens the POC scene

## Demo scene

8×8 room of Floor prefabs, back-left and back-right walls, an interior wall at row 4.5 whose left half is `FadeSelf` and right half `TintOccluded` (via `<Param name="Mode">`), two pillars, a two-tile pit, two stair tiles, two knights. Three painted areas: a Walkable region over the room (its front edges have no walls), Blocked over the pit, Cost 3× over the stairs.

- Right-click north of the interior wall: knights path around pit and pillars, avoid stairs unless it's worth 3×, and as they pass behind the wall the left segments fade while the right ones tint the knight blue.
- Hover: green crosshair on walkable ground, red off it; note the `agentRadius` margin around pillars and along walls.
- In the editor, drag a Wall or Pillar: the overlay rebakes as you move it. Tweak `cellSize`/`agentRadius` in the Systems tab and watch the same.

## Authoring rules

- Anchor (`TransformComponent`) is the **ground point** used for Y-sorting. Set the sprite `RectangleComponent.originY` so the footprint line/centre lands on it (walls: footprint centre; floors: diamond centre).
- Footprint = collider = nav area for most solid prefabs; author the point list once and paste it into each. Floors: nav area only. Decor: occluder only.
- Wall `height` ≈ how far the painted face rises above the footprint line, in px.
- Split long walls into tile-edge segments (as here); one anchor for an 8-tile wall Y-sorts wrong at its ends.

## Known gaps / next steps

1. **Editing collider/occluder polygons** — still text fields in the Inspector. `PolygonHandles` is ready to drive them from the viewport (and should call `SyncBoxToPolygon`, which text edits skip until reload).
2. **Isometric flag** — `RenderContext::IsIsometric()` still comes from `TileComponent`; irrelevant to this branch's SDF lighting as far as I can see, but worth moving to `SceneConfiguration` when tiles go.
3. **Polygon-vs-polygon collision** — entity-vs-entity uses the derived AABB. Fine for units and pillars; SAT only if diagonal walls need physical response rather than navmesh clamping.
4. **Occlusion cost** — O(dynamic × occluders) with a bounds pre-check. Fine for hundreds of prefabs; bucket statics by cell if scenes get large.
5. **Occludee shape** — a character is a vertical line at its anchor. Wide sprites at a wall's end may pop; sample at ±half width if seen.
6. **`SpatialSystem`** — keep only the spatial-hash half once tilemap scenes are gone.
7. **Lit layers** — the placeholder art has no normal maps; the entity layer's lighting will look flat on walls until painted assets ship normals like the Knight does.

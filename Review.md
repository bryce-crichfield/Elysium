# Code Review: Layer System Refactor

## Analysis
The recent changes refactor the layer system from an entity-based definition model to a scene-configuration model. This is a significant architectural improvement that separates scene data (layers) from game entities.

### Core Changes
*   **Layer Definition:** Layers are now defined in `SceneConfiguration` (and XML) rather than being entities with a `LayerComponent`.
*   **Layer Assignment:** `LayerComponent` is now a simple tag component containing a `name` string, used to assign entities to layers.
*   **Service Renaming:** `WorldService` has been renamed to `EditorService`, which better aligns with its responsibility of facilitating editor interactions.
*   **Editor Updates:** `SceneEditor` has been updated to support the new layer configuration workflow.

### Code Quality Concerns

#### 1. CRITICAL: RenderSystem::ComputeBounds is Commented Out
In `Source/Systems/RenderSystem.cpp`, the `ComputeBounds` method and its call within `RenderSingleItem` are commented out.
```cpp
// ComputeBounds(item.entity, item, layer);
```
and
```cpp
// void RenderSystem::ComputeBounds(Entity entity, const RenderItem& item, const LayerComponent& layer) { ... }
```
**Impact:** `BoundsComponent`s are no longer being updated with the visual dimensions of the entity. This will break:
*   Mouse picking/selection in the editor.
*   Any gameplay logic relying on visual bounds (collisions, UI interactions).

**Recommendation:** Restore `ComputeBounds`. It may need to be updated to accept `SceneLayer` instead of `LayerComponent` if parallax calculations were involved, but for basic bounds (width/height), the logic should remain largely the same.

#### 2. DebugSystem Implementation
*   `DebugSystem` is added but `Draw` is only declared in the header. Ensure the implementation exists in `Source/Systems/DebugSystem.cpp`.
*   Ensure `DebugSystem` is properly registered in the `Application` or `Scene` setup (it appears to be added in `Application::SetMode`).

#### 3. Lua API Breaking Changes
*   `CircleComponent`, `SpriteComponent`, `TextComponent`, etc., lost their `layerName` property.
*   **Mitigation:** The `test.lua` script was updated, but any other existing scripts will break. Ensure this breaking change is documented or a compatibility layer is added (e.g., in the Lua binding, if `layerName` is passed, automatically add a `LayerComponent`).
*   The `SceneLoader` has a backward compatibility fix for XML, which is good.

#### 4. Potential Null References
*   `RenderSystem::Draw` checks for camera existence, which is good.
*   `PickSystem` assumes `scene` is available.
*   Ensure `Scene::GetLayer(name)` handles non-existent layers gracefully (returns `nullptr` or default).

### Style & Conventions
*   The code generally follows the project's style.
*   New files (`SceneLayer.h`, `EditorService.h`) follow naming conventions.

---

## Patch Notes

### Architecture
*   **Layer System Refactor:** Layers are no longer defined by entities. They are now part of the `Scene` configuration.
*   **Scene Configuration:** Added `SceneConfiguration` struct to hold scene-wide settings like resolution and layers.
*   **Service Update:** Renamed `WorldService` to `EditorService`.

### Features
*   **Scene Editor:** Added tabs for Scenes, Scene Settings (Layers), and Systems.
*   **Layers:** Layers can now be configured (Z-index, Blend Mode, Space) directly in the Scene Editor.
*   **Scripting:** Added `ScriptService::SetActiveWorld` to improve script context handling.

### API Changes (Breaking)
*   **Lua:** `layerName` property removed from visual components (`Sprite`, `Circle`, `Text`, etc.).
    *   *New Usage:* `Component.Add(entity, "Layer", { name = "myLayer" })`
*   **C++:** `LayerComponent` is now a simple wrapper for a layer name string.
*   **XML:** `<Layers>` section in Scene XML is replaced by `<SceneConfiguration>`. (Loader includes backward compatibility for entity attributes but not the main layer definition block).

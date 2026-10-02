#pragma once

#include <string>
#include <unordered_map>

#include "Core/Entity.h"
#include "Core/Value.h"

namespace Elysium {

class ServiceLocator;
class Shader;
struct BoundsComponent;
struct MaterialComponent;
struct MovementComponent;
struct ParentComponent;
struct PolygonComponent;
struct PrefabInstanceComponent;
struct ScriptComponent;
struct ShaderComponent;
struct SpriteComponent;
struct UiComponent;

// The hand-written component inspectors, one file each in this folder. Every other component
// is inspected through its typed fields. RegisterComponentInspectors (Inspector.h) wires them up.
void InspectBounds(BoundsComponent& c, Entity e, ServiceLocator& services);
void InspectMaterial(MaterialComponent& c, Entity e, ServiceLocator& services);
void InspectMovement(MovementComponent& c, Entity e, ServiceLocator& services);
void InspectParent(ParentComponent& c, Entity e, ServiceLocator& services);
void InspectPolygon(PolygonComponent& c, Entity e, ServiceLocator& services);
void InspectPrefabInstance(PrefabInstanceComponent& c, Entity e, ServiceLocator& services);
void InspectScript(ScriptComponent& c, Entity e, ServiceLocator& services);
void InspectShader(ShaderComponent& c, Entity e, ServiceLocator& services);
void InspectSprite(SpriteComponent& c, Entity e, ServiceLocator& services);
void InspectUi(UiComponent& c, Entity e, ServiceLocator& services);

// One row per non-built-in uniform `shader` declares, showing the value in effect (override or
// source default); editing creates an override, Reset drops it. Shared by the Shader and
// Material inspectors.
void InspectUniformOverrides(const Shader& shader, std::unordered_map<std::string, Value>& overrides);

}  // namespace Elysium

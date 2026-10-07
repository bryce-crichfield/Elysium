#include "Editor/Inspectors/Inspector.h"

#include "Core/Components.h"
#include "Editor/Inspectors/ComponentInspectors.h"

namespace Elysium {

// Every component the Inspector shows, by section. A component listed with a function has a
// hand-written inspector (ComponentInspectors.h); the rest are drawn from their typed fields.
// A component left out of this table has no Inspector section.
void RegisterComponentInspectors(InspectorRegistry& registry) {
    registry.Register<NameComponent>(InspectorOrder::Identity);
    registry.Register<PrefabInstanceComponent>(InspectorOrder::Identity, &InspectPrefabInstance);

    registry.Register<TransformComponent>(InspectorOrder::Transform);

    registry.Register<ParentComponent>(InspectorOrder::Hierarchy, &InspectParent);

    registry.Register<BoundsComponent>(InspectorOrder::Geometry, &InspectBounds);
    registry.Register<CircleComponent>(InspectorOrder::Geometry);
    registry.Register<EllipseComponent>(InspectorOrder::Geometry);
    registry.Register<LineComponent>(InspectorOrder::Geometry);
    registry.Register<ModelComponent>(InspectorOrder::Geometry);
    registry.Register<AnimationComponent>(InspectorOrder::Geometry);
    registry.Register<PolygonComponent>(InspectorOrder::Geometry, &InspectPolygon);
    registry.Register<RectangleComponent>(InspectorOrder::Geometry);
    registry.Register<SpriteComponent>(InspectorOrder::Geometry, &InspectSprite);
    registry.Register<TextComponent>(InspectorOrder::Geometry);

    registry.Register<LayerComponent>(InspectorOrder::Layer);

    registry.Register<MaterialComponent>(InspectorOrder::Material, &InspectMaterial);

    registry.Register<ShaderComponent>(InspectorOrder::Shader, &InspectShader);

    registry.Register<CameraComponent>(InspectorOrder::Rendering);
    registry.Register<LightComponent>(InspectorOrder::Rendering);
    registry.Register<RevealComponent>(InspectorOrder::Rendering);
    registry.Register<UiComponent>(InspectorOrder::Rendering, &InspectUi);

    registry.Register<ColliderComponent>(InspectorOrder::Physics);
    registry.Register<FollowComponent>(InspectorOrder::Physics);
    registry.Register<KinematicsComponent>(InspectorOrder::Physics);
    registry.Register<MovementComponent>(InspectorOrder::Physics, &InspectMovement);
    registry.Register<NavAreaComponent>(InspectorOrder::Physics);

    registry.Register<AttackComponent>(InspectorOrder::Gameplay);
    registry.Register<HealthComponent>(InspectorOrder::Gameplay);
    registry.Register<TeamComponent>(InspectorOrder::Gameplay);

    registry.Register<ScriptComponent>(InspectorOrder::Scripting, &InspectScript);
}

}  // namespace Elysium

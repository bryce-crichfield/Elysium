#include "Component.h"

namespace Elysium
{
NameComponent::NameComponent(const std::string& name) : name(name) {}

PositionComponent::PositionComponent(float x, float y) : x(x), y(y)
{
}

LocationComponent::LocationComponent(int x, int y) : x(x), y(y)
{
}

MovementComponent::MovementComponent(const std::vector<Vector2> &waypoints) : waypoints(waypoints)
{
}

void MovementComponent::AddWaypoint(const Vector2& waypoint)
{
    waypoints.push_back(waypoint);
}

void MovementComponent::ClearWaypoints()
{
    waypoints.clear();
}

AnimationComponent::AnimationComponent(std::string marker) : marker(marker)
{
}

LayerComponent::LayerComponent(int z) : zIndex(z), type(Type::World), space(Space::World), blend(Blend::Normal)
{
}

RectangleComponent::RectangleComponent(float width, float height, Color background, Color border,
                                       const std::string &layer)
    : width(width), height(height), background(background), border(border), layerName(layer)
{
}

CircleComponent::CircleComponent(float r, Color background, Color border, const std::string &layer)
    : radius(r), background(background), border(border), layerName(layer)
{
}

SpriteComponent::SpriteComponent(const Sprite &sprite, const std::string &marker, const std::string &layer)
    : sprite(sprite), markerName(marker), layerName(layer)
{
}

TextureComponent::TextureComponent(const std::string &texture, const std::string &layer)
    : textureName(texture), layerName(layer)
{
}

TextComponent::TextComponent(const std::string &text, int size, Color c, const std::string &layer)
    : content(text), fontSize(size), color(c), layerName(layer)
{
}

TeamComponent::TeamComponent() : teamId(0)
{
}
TeamComponent::TeamComponent(int teamId) : teamId(teamId)
{
}

CameraComponent::CameraComponent()
    : zoom(1.0f), viewport({0, 0, 800, 600}), renderOrder(0), isVisible(true)
{
}

CooldownComponent::CooldownComponent() {}
CooldownComponent::CooldownComponent(float cooldown) : cooldownTime(cooldown) {}
void CooldownComponent::SetCooldown(float duration) {
    cooldownTime = duration;
    elapsedTime = 0.0f;
    isOnCooldown = true;
}
} // namespace Elysium
